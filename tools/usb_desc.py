from dataclasses import dataclass, astuple
import struct

#### Base
# https://www.beyondlogic.org/usbnutshell/usb5.shtml#DeviceDescriptors
@dataclass
class DeviceDescriptor:
    bLength: int
    bDescriptorType: int
    bcdUSB: int
    bDeviceClass: int
    bDeviceSubClass: int
    bDeviceProtocol: int
    bMaxPacketSize: int
    idVendor: int
    idProduct: int
    bcdDevice: int
    iManufacturer: int
    iProduct: int
    iSerialNumber: int
    bNumConfigurations: int

    def to_bytes(self):
        self.bLength = 18
        self.bDescriptorType = 1
        return struct.pack("<BBHBBBBHHHBBBB", *astuple(self))

@dataclass
class ConfigurationDescriptor:
    bLength: int
    bDescriptorType: int
    wTotalLength: int
    bNumInterfaces: int
    bConfigurationValue: int
    iConfiguration: int
    bmAttributes: int
    bMaxPower: int

    def to_bytes(self):
        self.bLength = 9
        self.bDescriptorType = 2
        return struct.pack("<BBHBBBBB", *astuple(self))

@dataclass
class InterfaceDescriptor:
    bLength: int
    bDescriptorType: int
    bInterfaceNumber: int
    bAlternateSetting: int
    bNumEndpoints: int
    bInterfaceClass: int
    bInterfaceSubClass: int
    bInterfaceProtocol: int
    iInterface: int

    def to_bytes(self):
        self.bLength = 9
        self.bDescriptorType = 4
        return struct.pack("BBBBBBBBB", *astuple(self))

@dataclass
class EndpointDescriptor:
    bLength: int
    bDescriptorType: int
    bEndpointAddress: int
    bmAttributes: int
    wMaxPacketSize: int
    bInterval: int

    def to_bytes(self):
        self.bLength = 7
        self.bDescriptorType = 5
        return struct.pack("<BBBBHB", *astuple(self))

@dataclass
class StringDescriptor:
    bLength: int
    bDescriptorType: int
    data: bytes

    def to_bytes(self):
        self.bLength = 2 + len(self.data)
        self.bDescriptorType = 3
        return struct.pack("BB" + "B" * len(self.data),
            *(astuple(self)[:2]), *self.data)

#### InterfaceAssociationDescriptor_ecn (USB 2.0)
@dataclass
class InterfaceAssociationDescriptor:
    bLength: int
    bDescriptorType: int
    bFirstInterface: int
    bInterfaceCount: int
    bFunctionClass: int
    bFunctionSubClass: int
    bFunctionProtocol: int
    iFunction: int

    def to_bytes(self):
        self.bLength = 8
        self.bDescriptorType = 0x0b
        return struct.pack("BBBBBBBB", *astuple(self))

#### USBCDC 1.2
@dataclass
class CDCHeaderFunctionalDescriptor:
    bFunctionLength: int
    bDescriptorType: int
    bDescriptorSubtype: int
    bcdCDC: int

    def to_bytes(self):
        self.bFunctionLength = 5
        self.bDescriptorType = 0x24
        self.bDescriptorSubtype = 0x0
        return struct.pack("<BBBH", *astuple(self))

@dataclass
class CDCUnionInterfaceFunctionalDescriptor:
    bFunctionLength: int
    bDescriptorType: int
    bDescriptorSubtype: int
    bControlInterface: int
    bSubordinateInterfaces: List[int]

    def to_bytes(self):
        self.bFunctionLength = 4 + len(self.bSubordinateInterfaces)
        self.bDescriptorType = 0x24
        self.bDescriptorSubtype = 0x6
        return struct.pack("BBBB" + "B"*len(self.bSubordinateInterfaces),
            *(astuple(self)[:4]), *self.bSubordinateInterfaces)

#### CDC PSTN Subclass rev. 1.2
@dataclass
class CallManagementFunctionalDescriptor:
    bFunctionLength: int
    bDescriptorType: int
    bDescriptorSubtype: int
    bmCapabilities: int
    bDataInterface: int

    def to_bytes(self):
        self.bFunctionLength = 5
        self.bDescriptorType = 0x24
        self.bDescriptorSubtype = 0x1
        return struct.pack("BBBBB", *astuple(self))

@dataclass
class CDCACMFunctionalDescriptor:
    bFunctionLength: int
    bDescriptorType: int
    bDescriptorSubtype: int
    bmCapabilities: int

    def to_bytes(self):
        self.bFunctionLength = 4
        self.bDescriptorType = 0x24
        self.bDescriptorSubtype = 0x2
        return struct.pack("BBBB", *astuple(self))

#### Glue
def assemble_configuration_descriptor(
    configuration_descriptor,
    interface_endpoint_descriptors):
    configuration_descriptor.wTotalLength = \
        9 + sum(len(i.to_bytes()) for i in interface_endpoint_descriptors)
    return bytes().join(i.to_bytes() for i in [configuration_descriptor] + interface_endpoint_descriptors)

def u16_to_bytes(x):
    return bytes([x & 0xff, (x >> 8) & 0xff])

def gen_endpoint_config(config_bytes):
    i = 0
    while i < len(config_bytes):
        descriptor_size = config_bytes[i]
        descriptor_type = config_bytes[i + 1]
        if descriptor_type == 5: # Endpoint
            endpoint_address = (config_bytes[i + 2] & 0xf)
            endpoint_direction = ((config_bytes[i + 2]) & 0x80 != 0)
            packet_size = config_bytes[i + 4] + (config_bytes[i + 5] << 8)
            yield (endpoint_address, endpoint_direction, packet_size)
        # Skip the non-endpoint descriptors
        i += descriptor_size

####

device = DeviceDescriptor(
    bLength=0,
    bDescriptorType=0,
    bcdUSB=0x0200, # USB 2.0
    bDeviceClass=0x0, # Each interface has its own class
    bDeviceSubClass=0x0,
    bDeviceProtocol=0x0,
    bMaxPacketSize=8,
    idVendor=0x0666,
    idProduct=0x4242,
    bcdDevice=0x0,
    iManufacturer=1, # String descriptor number
    iProduct=2, # String descriptor number
    iSerialNumber=3, # String descriptor number
    bNumConfigurations=1
)

config0 = ConfigurationDescriptor(
    bLength=0,
    bDescriptorType=0,
    wTotalLength=0,
    bNumInterfaces=2,
    bConfigurationValue=1,
    iConfiguration=4, # String descriptor number
    bmAttributes=0x80, # D7=RES1
    bMaxPower=50 # 100mA max
)
config0_iad = InterfaceAssociationDescriptor(
    bLength=0,
    bDescriptorType=0,
    bFirstInterface=0,
    bInterfaceCount=2,
    bFunctionClass=0x2, # CDC
    bFunctionSubClass=0x2, # ACM
    bFunctionProtocol=0x0,
    iFunction=5 # String descriptor number
)
config0_interface0 = InterfaceDescriptor(
    bLength=0,
    bDescriptorType=0,
    bInterfaceNumber=0,
    bAlternateSetting=0,
    bNumEndpoints=1, # Optional notification endpoint. Defined when bmCapabilities[1]=1
                     # in CDCACMFunctionalDescriptor
    bInterfaceClass=0x2, # Communications and CDC Control
    bInterfaceSubClass=0x2, # ACM
    bInterfaceProtocol=0x0,
    iInterface=6 # String descriptor number
)
config0_cdc_header_functional = CDCHeaderFunctionalDescriptor(
    bFunctionLength=0,
    bDescriptorType=0,
    bDescriptorSubtype=0,
    bcdCDC=0x0120
)
config0_cdc_call_management_functional = CallManagementFunctionalDescriptor(
    bFunctionLength=0,
    bDescriptorType=0,
    bDescriptorSubtype=0,
    bmCapabilities=0x0, # Don't really use call management
    bDataInterface=0x1 # Points to config0_interface1
)
config0_cdc_acm_functional = CDCACMFunctionalDescriptor(
    bFunctionLength=0,
    bDescriptorType=0,
    bDescriptorSubtype=0,
    bmCapabilities=0x2 # Support SET_LINE_CODING, GET_LINE_CODING, SET_CONTROL_LINE_STATE
                       # (Not truly used here, but might be useful for drivers that
                       # really want to send these commands)
)
config0_cdc_union_interface_functional = CDCUnionInterfaceFunctionalDescriptor(
    bFunctionLength=0,
    bDescriptorType=0,
    bDescriptorSubtype=0,
    bControlInterface=0,
    bSubordinateInterfaces=[1]
)
config0_interface0_endpoint0 = EndpointDescriptor(
    bLength=0,
    bDescriptorType=0,
    bEndpointAddress=0x81, # IN, address=1
    bmAttributes=0x03, # Interrupt
    wMaxPacketSize=8,
    bInterval=255 # Take the longest interval (255ms) as this endpoint doesn't matter
                  # much
)
config0_interface1 = InterfaceDescriptor(
    bLength=0,
    bDescriptorType=0,
    bInterfaceNumber=1,
    bAlternateSetting=0,
    bNumEndpoints=2, # BULK IN and BULK OUT, same EP address
    bInterfaceClass=0xa, # Data interface
    bInterfaceSubClass=0x0,
    bInterfaceProtocol=0x0,
    iInterface=7 # String descriptor number
)
config0_interface1_endpoint0 = EndpointDescriptor(
    bLength=0,
    bDescriptorType=0,
    bEndpointAddress=0x82, # IN, address=2
    bmAttributes=0x02, # Bulk endpoint
    wMaxPacketSize=16,
    bInterval=0 # Ignored
)
config0_interface1_endpoint1 = EndpointDescriptor(
    bLength=0,
    bDescriptorType=0,
    bEndpointAddress=0x02, # OUT, address=2
    bmAttributes=0x02, # Bulk endpoint
    wMaxPacketSize=8,
    bInterval=0 # Ignored
)

strings = [
    StringDescriptor(
        bLength=0,
        bDescriptorType=0,
        data=u16_to_bytes(0x041d)
    )
]
strings += list(
    StringDescriptor(
        bLength=0,
        bDescriptorType=0,
        data=s.encode("utf-16")
    ) for s in [
        # Manufacturer
        "Manufacturer \U0001f3f4\u200d\u2620\ufe0f",
        # Product
        "e-nez",
        # Serial number
        "0000-0000-0000-0000",
        # Config0
        "Terminal",
        # Config0.IAD
        "CDC ACM",
        # Config0.Interface0
        "CDC ACM control",
        # Config0.Interface1
        "CDC ACM data",
    ]
)

assembled_config0 = assemble_configuration_descriptor(
    config0,
    [
         config0_iad,
         config0_interface0,
         config0_cdc_header_functional, config0_cdc_union_interface_functional,
         config0_cdc_call_management_functional, config0_cdc_acm_functional,
         config0_interface0_endpoint0,
         config0_interface1, config0_interface1_endpoint0,
            config0_interface1_endpoint1
    ]
)
