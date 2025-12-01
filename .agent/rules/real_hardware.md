To test on the real hardware, prepare cross-comiled binaries:
`export PATH=/opt/xb1/bin:$PATH; CROSS_COMPILE=mipsel-linux- ./build.sh`

Copy the binaries to the network share:
`cp onvif_notify_server onvif_simple_server wsd_simple_server /nfs/`

Mount the share on the camera
```
ssh root@[camera ip address]

# then on the camera
mount -o nolock 192.168.88.20:/nfs /mnt/nfs
mount --bind /mnt/nfs/onvif_simple_server /usr/sbin/onvif.cgi
mount --bind /mnt/nfs/onvif_notify_server /usr/sbin/onvif_notify_server
mount --bind /mnt/nfs/wsd_simple_server   /usr/sbin/wsd_simple_server
```