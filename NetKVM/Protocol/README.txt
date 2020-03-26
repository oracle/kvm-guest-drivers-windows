Install/TODO/History
Annie Li(annie.li@oracle.com)

To install and test 2-netdev model manually

1. Boot up VM with VirtIO and VF device which share same MAC address
2. Copy following installation binaries into folder "install"
   take win8 release for an example
   x64/win8release/netkvm.inf, netkvm.sys
   NotifyObject/x64/win8release/vioprot.inf, netkvmno.dll
3. Go to Device manager-> Other devices-> right click Ethernet Controller
   -> Update Driver Software
4. Browse my computer for device software
5. Browse folder "install"  which contains drivers -> Next
   -> Install driver anyway
6. Right click network icon on bottom-left corner
7. Open network and sharing center
8. Click one of existing connections -> Property -> Install -> Protocol
   -> Add -> Have Disk -> Browse the folder which contains installation files
   -> Select vioprot.inf -> OK -> VirtIO Protocol Driver -> OK
9. Reboot VM
10. Ip link set tap device down to disable VirtIO connection
11. Disable VF in Device manager and enable VirtIO connection through ip link
    command or delete VF device on the host
12. Verify network connection through VirtIO
13. Disable VirtIO network connection and enable VF in Device manager
14. Verify network connection through VF

Note
SR-IOV on Windows 2012 is tested during deployment, user may run into error
"0x800700C1" when installing protocol driver, this is due to missing of VC++
2015 redistributable on Windows 2012. Install this redistributable to fix this
error on Windows 2012.

Current status

Switch from virtio to VF - network connection comes back pretty fast.
Switch from VF to virtio - network connection comes back pretty fast(this
could be verified by pinging outside from VM)

TODO
1. Add support for WinXP and Win2003
2. Current notify object only binds Intel VF(device ID 0x1515) in 1 to 1 mode,
   To support VFs from other venders, their VF device ID need to be added into
   the array.
3. Implement new installer feature to support install/update/uninstall protocol
   driver and notify object.
4. Test performance and latency of VF in this 2-netdev model in 1G/10G/40G/100G
   environment.
5. Check VF network property in 2-netdev model, for example, offload, RSS, etc.
   If some properties get lost, it is necessary to implement it, or keep VF's
   property as most as we can.
6. WHQL test
   netkvm.sys driver binary combines both network miniport driver and protocol
   driver, and it isn't standard network miniport driver anymore. To pass WHQL
   test QA need to run WHQL test case for both miniport and protocol driver.
   The protocol driver isn't standard protocol driver, it is necessary to
   consult MS about passing WHQL test on this protocol driver. Hyper-V uses same
   solution, so maybe this isn't a big issue. Passing WHQL test on protocol
   driver may cause lots of time because it is new driver and may have WHQL bugs
   before it becomes stable, and this involves more back and forth rounds
   between dev and QA.
7. Combine winvirtio 2-netdev model with qemu command together for Cloud usage,
   and simulate Cloud environment to test SR-IOV live migration.
8. Loading order of netkvm and VF driver may affect path switching between
   drivers and whether network comes back immediately after guest boots up, this
   part needs to be investigated.
9. Verify network packets statistics of VF in 2-netdev model.
10.Implement queue mechanism for both Tx and Rx? This protocol driver is different
   from the normal protocol driver, whether supporting queue is to be investigated.
11.Deal with remaining data for NetKVM DPC routine after switching from VirtIO to VF.


History

02/18/2020
Fix two existing issues,
1. Sometimes network connection come back slowly after switching from VF
   path to VirtIO path.
2. VirtIO network connection fails randomly without SRIOV enabled.

06/14/2019
Implemented prototype of 2-netdev SRIOV live migration on Windows.
