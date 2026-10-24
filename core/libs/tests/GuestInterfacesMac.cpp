#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <net/if.h>

struct GuestInterfaceAddress {
    GuestInterfaceAddress* next;
    char* name;
    std::uint32_t flags;
    void* address;
    void* netmask;
    void* destination;
    void* data;
};

extern "C" {
int APS5_VABI getifaddrs_nid_postfix(GuestInterfaceAddress** list);
void APS5_VABI freeifaddrs_nid_postfix(GuestInterfaceAddress* list);
}

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    std::array<void*, 10> api{};
    ApplicationHeapRegister_nid_no_patch(api.data());
    GuestInterfaceAddress* list = nullptr;
    Require(getifaddrs_nid_postfix(&list) == 0);
    bool foundLoopbackLink = false;
    bool foundLoopbackAddress = false;
    for (auto* item = list; item; item = item->next) {
        Require(item->name && item->address);
        if (std::strcmp(item->name, "lo0") != 0) continue;
        const auto* address = static_cast<const std::uint8_t*>(item->address);
        if (address[1] == 18) {
            std::uint16_t index = 0;
            std::memcpy(&index, address + 2, sizeof(index));
            Require(index == if_nametoindex("lo0"));
            Require(address[4] == 0x18 && address[6] == 0);
            Require(item->data && !item->netmask && !item->destination);
            foundLoopbackLink = true;
        } else if (address[1] == 2 || address[1] == 28) {
            Require(item->netmask && !item->data);
            foundLoopbackAddress = true;
        }
    }
    freeifaddrs_nid_postfix(list);
    Require(foundLoopbackLink && foundLoopbackAddress);
}
