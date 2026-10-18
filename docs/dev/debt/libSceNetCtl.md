# libSceNetCtl technical debt

### Silent stubs

- [sceNetCtlGetNatInfo](../../../core/libs/prx/libSceNetCtl/Export.cpp) (libSceNetCtl) - `sceNetCtlGetNatInfo` validates null pointers and returns `SCE_NET_CTL_ERROR_NOT_CONNECTED` (`0x80412108`) without performing NAT discovery.

### Unknown function info

- [sceNetCtlGetInfo](../../../core/libs/prx/libSceNetCtl/Export.cpp) (libSceNetCtl) - answers from the host interface that holds an IPv4 address, preferring the one that carries the default route: its address, netmask, gateway, MTU and first two IPv4 DNS servers. The device is reported as wired, the link as connected, the IP configuration as DHCP and the HTTP proxy as off, whatever the host uses, and the Ethernet address is the one `sceNetGetMacAddress` reports. A gateway or DNS server the interface does not have is reported as `0.0.0.0`; what a console reports then is unknown. The DHCP host name (12), the PPPoE user name (13) and the HTTP proxy server (20) are empty strings, as that configuration sets none of them; the HTTP proxy port (21) is 0; the Wi-Fi codes (5-10) throw

### Functional
- [sceNetCtlGetInfo](../../../core/libs/prx/libSceNetCtl/Export.cpp) (libSceNetCtl) finds the host's default route in `/proc/net/route` on Linux and, on macOS, in the routing table read with `sysctl(NET_RT_FLAGS, RTF_GATEWAY)`, where an interface-scoped default route ranks after the unscoped one
- [sceNetCtlGetIfStat](../../../core/libs/prx/libSceNetCtl/Export.cpp) (libSceNetCtl) on macOS sums the `AF_LINK` byte counters of the interfaces that are up and not loopback, which an unprivileged process reads (`getifaddrs` and `sysctl(NET_RT_IFLIST2)` alike) reduced to their low 32 bits, so each interface's total wraps every 4 GiB
