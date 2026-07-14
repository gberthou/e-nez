import sys

import usb_desc

USBSRAM_BASE = 0x40009840
USBSRAM_SIZE = (2 << 10) - 0x40

EP0_PACKET_SIZE = usb_desc.device.bMaxPacketSize

HEADER_GUARD = "BOARD_USB_ENDPOINTS_H"

class Allocator:
    def __init__(self, base, size, log2_alignment):
        self.address = base
        self.end = base + size
        self.alignment = (1 << log2_alignment)

    def allocate(self, size):
        address = self.address
        size = (size + self.alignment - 1) & ~(self.alignment - 1)
        self.address += size
        if self.address > self.end:
            raise RuntimeError("Requested packet sizes cannot fit in USBSRAM")
        return address

def pointer_to_str(constant_name, value, definition):
    if definition:
        ret = ""
    else:
        ret = "extern "
    ret += "volatile void * const " + constant_name
    if definition:
        ret += " = (volatile void*) " + hex(value)
    ret += ";\n"
    return ret

def size_to_h(constant_name, value):
    return "constexpr uint16_t " + constant_name + " = " + str(value) + ";\n"

control_allocator = Allocator(USBSRAM_BASE, USBSRAM_SIZE, 3)
ep0_tx_address = control_allocator.allocate(EP0_PACKET_SIZE)
ep0_rx_address = control_allocator.allocate(EP0_PACKET_SIZE)

basename = str(sys.argv[1])
with open(basename + ".c", "w") as fc, \
    open(basename + ".h", "w") as fh:
    fc.write(pointer_to_str("ep0_tx_pkt", ep0_tx_address, True))
    fc.write(pointer_to_str("ep0_rx_pkt", ep0_rx_address, True))

    fh.write("#ifndef " + HEADER_GUARD + "\n")
    fh.write("#define " + HEADER_GUARD + "\n")
    fh.write("#include <stdint.h>\n")
    fh.write(pointer_to_str("ep0_tx_pkt", ep0_tx_address, False))
    fh.write(size_to_h("ep0_tx_size", EP0_PACKET_SIZE))
    fh.write(pointer_to_str("ep0_rx_pkt", ep0_rx_address, False))
    fh.write(size_to_h("ep0_rx_size", EP0_PACKET_SIZE))

    for config_index, config_bytes in enumerate([usb_desc.assembled_config0]):
        # USB packet addresses should be aligned to 8B, cf. Sections 2.1/3.1 of
        # https://community.st.com/stm32-mcus-60/how-to-configure-the-packet-memory-area-in-stm32-usb-controllers-156323
        allocator = Allocator(USBSRAM_BASE + 2 * EP0_PACKET_SIZE, USBSRAM_SIZE,
            3) # 8B alignment

        known_address_direction = set()
        for address, direction, packet_size in usb_desc.gen_endpoint_config(config_bytes):
            if (address, direction) in known_address_direction:
                raise RuntimeError("Endpoint " + str(address) + ("IN" if direction else "OUT") + " is referenced several times")
            known_address_direction.add((address, direction))

            allocated_address = allocator.allocate(packet_size)
            name_prefix = "config" + str(config_index) + "_ep" + str(address) + "_" \
                + ("tx" if direction else "rx")
            packet_constant_name = name_prefix + "_pkt"
            size_constant_name = name_prefix + "_size"

            fc.write(pointer_to_str(packet_constant_name, allocated_address, True))
            fh.write(pointer_to_str(packet_constant_name, allocated_address, False))
            fh.write(size_to_h(size_constant_name, packet_size))

    fh.write("#endif // " + HEADER_GUARD)
