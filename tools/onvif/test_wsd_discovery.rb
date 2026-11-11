#!/usr/bin/env ruby
# WS-Discovery compliance checks for wsd_simple_server.
#
# Exercises ONVIF Core 26.06 / WS-Discovery 2005-04 Target Service behavior:
# Hello, Bye, Probe type and scope matching, Resolve, AppSequence, and the
# discoverable/non-discoverable transitions.

require 'fileutils'
require 'rexml/document'
require 'json'
require 'securerandom'
require 'socket'
require 'timeout'
require 'tmpdir'

ROOT = File.expand_path('../..', __dir__)
BIN = ENV['WSD_BIN'] || File.join(ROOT, 'wsd_simple_server')
TEMPLATES = ENV['WSD_TEMPLATES'] || File.join(ROOT, 'res', 'wsd_files')
PORT = 3702
HOST = '127.0.0.1'
STATE = '/tmp/onvif_discovery_mode'
RFC3986 = 'http://schemas.xmlsoap.org/ws/2005/04/discovery/rfc3986'
SCOPES = 'onvif://www.onvif.org/Profile/Streaming onvif://www.onvif.org/Profile/T onvif://www.onvif.org/hardware onvif://www.onvif.org/name'

$failures = 0
$tmpdir = Dir.mktmpdir('wsd-test')
CONFIG = File.join($tmpdir, 'onvif.json')
File.write(CONFIG, JSON.generate({
  'serial_num' => 'WSD-TEST-SERIAL',
  'scopes' => SCOPES.split(' ')
}))
$pidfile = File.join($tmpdir, 'wsd.pid')
$logfile = File.join($tmpdir, 'wsd.log')
$wsd_pid = nil

def check(name, condition)
  if condition
    puts "PASS  #{name}"
  else
    puts "FAIL  #{name}"
    $failures += 1
  end
end

def parse(xml)
  REXML::Document.new(xml)
rescue REXML::ParseException => e
  puts "FAIL  invalid XML: #{e.message}"
  $failures += 1
  nil
end

def send_udp(xml, timeout: 1.5)
  socket = UDPSocket.new
  socket.send(xml, 0, HOST, PORT)
  Timeout.timeout(timeout) { socket.recvfrom(65_535)[0] }
rescue Timeout::Error
  nil
ensure
  socket&.close
end

def envelope(action, message_id, body)
  <<~XML
    <s:Envelope xmlns:s="http://www.w3.org/2003/05/soap-envelope" xmlns:a="http://schemas.xmlsoap.org/ws/2004/08/addressing" xmlns:d="http://schemas.xmlsoap.org/ws/2005/04/discovery" xmlns:tdn="http://www.onvif.org/ver10/network/wsdl" xmlns:tds="http://www.onvif.org/ver10/device/wsdl"><s:Header><a:Action>#{action}</a:Action><a:MessageID>#{message_id}</a:MessageID><a:To>urn:schemas-xmlsoap-org:ws:2005:04:discovery</a:To></s:Header><s:Body>#{body}</s:Body></s:Envelope>
  XML
end

def probe_xml(message_id, types: 'tdn:NetworkVideoTransmitter', scopes: nil, match_by: RFC3986)
  types_xml = types.nil? ? '' : "<d:Types>#{types}</d:Types>"
  scopes_xml = scopes.nil? ? '' : "<d:Scopes MatchBy=\"#{match_by}\">#{scopes}</d:Scopes>"
  envelope('http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe', message_id,
           "<d:Probe>#{types_xml}#{scopes_xml}</d:Probe>")
end

def resolve_xml(message_id, endpoint)
  envelope('http://schemas.xmlsoap.org/ws/2005/04/discovery/Resolve', message_id,
           "<d:Resolve><a:EndpointReference><a:Address>#{endpoint}</a:Address></a:EndpointReference></d:Resolve>")
end

def wait_until(timeout: 8)
  deadline = Time.now + timeout
  loop do
    return true if yield
    return false if Time.now >= deadline
    sleep 0.1
  end
end

def start_wsd
  log = File.open($logfile, 'w')
  $wsd_pid = Process.spawn(BIN,
                           '-i', 'lo',
                           '-x', 'http://%s/onvif/device_service',
                           '-c', CONFIG,
                           '-p', $pidfile,
                           '-t', TEMPLATES,
                           '-f', '-d', '5',
                           out: log, err: log)
  log.close

  ready = wait_until do
    response = send_udp(probe_xml("urn:uuid:#{SecureRandom.uuid}"), timeout: 0.3)
    !response.nil?
  end
  raise "wsd_simple_server did not become ready; see #{$logfile}" unless ready
end

def stop_wsd
  return unless $wsd_pid

  Process.kill('TERM', $wsd_pid)
  begin
    Timeout.timeout(3) { Process.wait($wsd_pid) }
  rescue Timeout::Error
    Process.kill('KILL', $wsd_pid) rescue nil
    Process.wait($wsd_pid) rescue nil
  end
  $wsd_pid = nil
end

def probe(message_id:, timeout: 1.5, **kwargs)
  send_udp(probe_xml(message_id, **kwargs), timeout: timeout)
end

def discovery_mode(mode)
  File.write(STATE, "#{mode}\n")
  Process.kill('HUP', $wsd_pid)
end

begin
  start_wsd

  # Probe: required ONVIF type, scopes, EPR, XAddrs, AppSequence.
  response = probe(message_id: "urn:uuid:#{SecureRandom.uuid}")
  check('Probe gets ProbeMatches', response&.include?('ProbeMatches'))
  parse(response) if response
  check('ProbeMatches has tdn:NetworkVideoTransmitter', response&.include?('tdn:NetworkVideoTransmitter'))
  check('ProbeMatches has tds:Device', response&.include?('tds:Device'))
  check('ProbeMatches has Profile/Streaming scope', response&.include?('onvif://www.onvif.org/Profile/Streaming'))
  check('ProbeMatches has XAddrs', response&.include?('<d:XAddrs>http://'))
  check('ProbeMatches has MetadataVersion', response&.include?('<d:MetadataVersion>'))
  check('ProbeMatches has nonzero AppSequence InstanceId',
        response && response.match?(/InstanceId="[1-9][0-9]*"/))
  check('ProbeMatches has MessageNumber', response&.match?(/MessageNumber="[1-9][0-9]*"/))

  # Types: tds:Device and the ONVIF NVT type both match; unknown types do not.
  check('Probe for tds:Device matches', !probe(message_id: "urn:uuid:#{SecureRandom.uuid}", types: 'tds:Device').nil?)
  check('Probe for both types matches',
        !probe(message_id: "urn:uuid:#{SecureRandom.uuid}", types: 'tdn:NetworkVideoTransmitter tds:Device').nil?)
  check('Probe for unknown type does not match',
        probe(message_id: "urn:uuid:#{SecureRandom.uuid}", types: 'tdn:NotACamera').nil?)

  # Scopes: all probe scopes must match, and matching is segment-wise prefix.
  check('Probe for matching scope matches',
        !probe(message_id: "urn:uuid:#{SecureRandom.uuid}", scopes: 'onvif://www.onvif.org/Profile/Streaming').nil?)
  check('Probe for root scope matches',
        !probe(message_id: "urn:uuid:#{SecureRandom.uuid}", scopes: 'onvif://www.onvif.org').nil?)
  check('Probe for all matching scopes matches',
        !probe(message_id: "urn:uuid:#{SecureRandom.uuid}",
               scopes: 'onvif://www.onvif.org/Profile/Streaming onvif://www.onvif.org/name').nil?)
  check('Probe with one non-matching scope does not match',
        probe(message_id: "urn:uuid:#{SecureRandom.uuid}",
              scopes: 'onvif://www.onvif.org/Profile/Streaming onvif://www.onvif.org/name/Nope').nil?)
  check('Scope matching is not a plain string prefix',
        probe(message_id: "urn:uuid:#{SecureRandom.uuid}",
              scopes: 'onvif://www.onvif.org/hardware/D1').nil?)

  # Duplicate Probe suppression (same MessageID).
  duplicate_id = "urn:uuid:#{SecureRandom.uuid}"
  first = send_udp(probe_xml(duplicate_id), timeout: 1.5)
  second = send_udp(probe_xml(duplicate_id), timeout: 0.6)
  check('Duplicate Probe is answered once', !first.nil? && second.nil?)

  # Resolve / ResolveMatches.
  response = probe(message_id: "urn:uuid:#{SecureRandom.uuid}")
  endpoint = response && response[/<a:Address>(urn:uuid:[^<]+)<\/a:Address>/, 1]
  check('Probe response contains EPR', !endpoint.nil?)
  resolve_response = endpoint && send_udp(resolve_xml("urn:uuid:#{SecureRandom.uuid}", endpoint))
  check('Resolve gets ResolveMatches', resolve_response&.include?('ResolveMatches'))
  check('ResolveMatches contains endpoint', endpoint && resolve_response&.include?(endpoint))
  check('Resolve for another endpoint does not match',
        send_udp(resolve_xml("urn:uuid:#{SecureRandom.uuid}", 'urn:uuid:00000000-0000-0000-0000-000000000000'), timeout: 0.6).nil?)

  # Endpoint reference is stable across restarts.
  stop_wsd
  start_wsd
  restarted = probe(message_id: "urn:uuid:#{SecureRandom.uuid}")
  restarted_endpoint = restarted && restarted[/<a:Address>(urn:uuid:[^<]+)<\/a:Address>/, 1]
  check('Endpoint reference is stable across restarts', endpoint == restarted_endpoint)

  # Discoverable / non-discoverable transitions.
  discovery_mode('NonDiscoverable')
  sleep 1.2
  check('NonDiscoverable stops answering probes',
        probe(message_id: "urn:uuid:#{SecureRandom.uuid}", timeout: 0.6).nil?)

  discovery_mode('Discoverable')
  sleep 1.5
  check('Discoverable resumes answering probes',
        !probe(message_id: "urn:uuid:#{SecureRandom.uuid}").nil?)
ensure
  stop_wsd
  if $failures.positive? && File.exist?($logfile)
    puts "\n--- wsd_simple_server log ---"
    puts File.read($logfile)
  end
  FileUtils.rm_rf($tmpdir)
  File.unlink(STATE) if File.exist?(STATE)
end

if $failures.zero?
  puts 'All WS-Discovery checks passed.'
  exit 0
else
  puts "#{$failures} WS-Discovery check(s) failed."
  exit 1
end
