from dataclasses import dataclass, astuple
import struct

HAS_CDC_ACM = False
AUDIO_SAMPLING_FREQUENCY_HZ = 44100

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

### USB Audio 2.0
@dataclass
class ACInterfaceHeaderDescriptor:
    bFunctionLength: int
    bDescriptorType: int
    bDescriptorSubtype: int
    bcdADC: int
    bCategory: int
    wTotalLength: int
    bmControls: int

    def to_bytes(self):
        self.bFunctionLength = 9
        self.bDescriptorType = 0x24 # CS_INTERFACE
        self.bDescriptorSubtype = 0x1 # HEADER
        return struct.pack("<BBBHBHB", *astuple(self))

@dataclass
class AudioClockSourceDescriptor:
    bLength: int
    bDescriptorType: int
    bDescriptorSubtype: int
    bClockID: int
    bmAttributes: int
    bmControls: int
    bAssocTerminal: int
    iClockSource: int

    def to_bytes(self):
        self.bLength = 8
        self.bDescriptorType = 0x24 # CS_INTERFACE
        self.bDescriptorSubtype = 0xa # CLOCK_SOURCE
        return struct.pack("BBBBBBBB", *astuple(self))

@dataclass
class AudioInputTerminalDescriptor:
    bLength: int
    bDescriptorType: int
    bDescriptorSubtype: int
    bTerminalId: int
    wTerminalType: int
    bAssocTerminal: int
    bCSourceID: int
    bNrChannels: int
    bmChannelConfig: int
    iChannelNames: int
    bmControls: int
    iTerminal: int

    def to_bytes(self):
        self.bLength = 17
        self.bDescriptorType = 0x24 # CS_INTERFACE
        self.bDescriptorSubtype = 0x2 # INPUT_TERMINAL
        return struct.pack("<BBBBHBBBIBHB", *astuple(self))

@dataclass
class AudioOutputTerminalDescriptor:
    bLength: int
    bDescriptorType: int
    bDescriptorSubtype: int
    bTerminalId: int
    wTerminalType: int
    bAssocTerminal: int
    bSourceID: int
    bCSourceID: int
    bmControls: int
    iTerminal: int

    def to_bytes(self):
        self.bLength = 12
        self.bDescriptorType = 0x24 # CS_INTERFACE
        self.bDescriptorSubtype = 0x3 # OUTPUT_TERMINAL
        return struct.pack("<BBBBHBBBHB", *astuple(self))

@dataclass
class AudioASInterfaceDescriptor:
    bLength: int
    bDescriptorType: int
    bDescriptorSubtype: int
    bTerminalLink: int
    bmControls: int
    bFormatType: int
    bmFormats: int
    bNrChannels: int
    bmChannelConfig: int
    iChannelNames: int

    def to_bytes(self):
        self.bLength = 16
        self.bDescriptorType = 0x24 # CS_INTERFACE
        self.bDescriptorSubtype = 0x1 # AS_GENERAL
        return struct.pack("<BBBBBBIBIB", *astuple(self))

@dataclass
class AudioASIsochronousDataEndpointDescriptor:
    bLength: int
    bDescriptorType: int
    bDescriptorSubtype: int
    bmAttributes: int
    bmControls: int
    bLockDelayUnits: int
    wLockDelay: int

    def to_bytes(self):
        self.bLength = 8
        self.bDescriptorType = 0x25 # CS_ENDPOINT
        self.bDescriptorSubtype = 0x1 # EP_GENERAL
        return struct.pack("<BBBBBBH", *astuple(self))

### Audio Data Formats 2.0
@dataclass
class AudioTypeIDescriptor:
    bLength: int
    bDescriptorType: int
    bDescriptorSubtype: int
    bFormatType: int
    bSubslotSize: int
    bBitResolution: int

    def to_bytes(self):
        self.bLength = 6
        self.bDescriptorType = 0x24 # CS_INTERFACE
        self.bDescriptorSubtype = 0x2 # FORMAT_TYPE
        self.bFormatType = 0x1 # Type I
        return struct.pack("BBBBBB", *(astuple(self)))

#### Glue
def assemble_configuration_descriptor(
    configuration_descriptor,
    interface_endpoint_descriptors):
    # Each element from interface_endpoint_descriptors can be already "assembled". In
    # that case, said elements are no longer instance of a descriptor dataclass, but
    # rather already processed as bytes()
    def length(x):
        if isinstance(x, bytes):
            return len(x)
        return len(x.to_bytes())

    def to_bytes(x):
        if isinstance(x, bytes):
            return bytes(x)
        return x.to_bytes()

    configuration_descriptor.wTotalLength = \
        9 + sum(length(i) for i in interface_endpoint_descriptors)
    return bytes().join(to_bytes(i) for i in [configuration_descriptor] + interface_endpoint_descriptors)

# Hack: Audio AC Header descriptor has the same size as the standard configuration
# descriptor and they both define the wTotalLength attribute
assemble_audio_ac_descriptor = assemble_configuration_descriptor

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
            endpoint_type = (config_bytes[i + 3] & 0x3)
            packet_size = config_bytes[i + 4] + (config_bytes[i + 5] << 8)
            yield (endpoint_address, endpoint_direction, endpoint_type, packet_size)
        # Skip the non-endpoint descriptors
        i += descriptor_size

####

device = DeviceDescriptor(
    bLength=0,
    bDescriptorType=0,
    bcdUSB=0x0200, # USB 2.0
    bDeviceClass=0xef, # Misc
    bDeviceSubClass=0x02,
    bDeviceProtocol=0x01, # IAD
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
    bNumInterfaces=2 + (2 if HAS_CDC_ACM else 0), # CDC ACM control, CDC ACM data
                                                  # Audio control, Audio stream
    bConfigurationValue=1,
    iConfiguration=4, # String descriptor number
    bmAttributes=0x80, # D7=RES1
    bMaxPower=50 # 100mA max
)
config0_iad_cdc_acm = InterfaceAssociationDescriptor(
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
config0_iad_audio = InterfaceAssociationDescriptor(
    bLength=0,
    bDescriptorType=0,
    bFirstInterface=(2 if HAS_CDC_ACM else 0),
    bInterfaceCount=2, # AC + AS
    bFunctionClass=0x1, # Audio
    bFunctionSubClass=0x0,
    bFunctionProtocol=0x20, # 2.0
    iFunction=8 # String descriptor number
)
config0_interface2 = InterfaceDescriptor(
    bLength=0,
    bDescriptorType=0,
    bInterfaceNumber=0 + (2 if HAS_CDC_ACM else 0),
    bAlternateSetting=0,
    bNumEndpoints=0, # 0 or 1 if optional interrupt endpoint is present
    bInterfaceClass=0x1, # Audio
    bInterfaceSubClass=0x1, # AUDIOCONTROL
    bInterfaceProtocol=0x20, # 2.0
    iInterface=9 # String descriptor number
)
config0_audio_control_interface_header = ACInterfaceHeaderDescriptor(
    bFunctionLength=0,
    bDescriptorType=0,
    bDescriptorSubtype=0,
    bcdADC=0x0200, # 2.0
    bCategory=0x9, # MUSICAL_INSTRUMENT
    wTotalLength=0,
    bmControls=0 # TODO: Maybe support latency control later?
)
config0_audio_clock_source = AudioClockSourceDescriptor(
    bLength=0,
    bDescriptorType=0,
    bDescriptorSubtype=0,
    bClockID=1, # Clock has id=1
    bmAttributes=0x1, # Internal fixed clock, synchronized to SOF
                      # D[1:0]: 00 = external clock
                      #         01 = internal fixed clock
                      #         10 = internal variable clock
                      #         11 = internal programmable clock
                      # D2: Synchronized to SOF
    bmControls=0x3, # Present, read-write
    bAssocTerminal=0, # Global
    iClockSource=10 # String descriptor number
)
config0_audio_input_terminal = AudioInputTerminalDescriptor(
    bLength=0,
    bDescriptorType=0,
    bDescriptorSubtype=0,
    bTerminalId=2,
    wTerminalType=0x0201, # Input terminal, microphone (seems better detected by
                          # Windows)
    bAssocTerminal=0, # Linked to output terminal (id=2)
    bCSourceID=1, # Clock
    bNrChannels=2, # Stereo
    bmChannelConfig=0x3,
    iChannelNames=0,
    bmControls=0x0, # D[ 1: 0] = Copy protect control
                    # D[ 3: 2] = Connector control
                    # D[ 5: 4] = Overload control
                    # D[ 7: 6] = Cluster control
                    # D[ 9: 8] = Underflow control
                    # D[11:10] = Overflow control
                    # D[13:11] = Phantom power control
                    # TODO: See if any feature is applicable
    iTerminal=11 # String descriptor number
)
config0_audio_output_terminal = AudioOutputTerminalDescriptor(
    bLength=0,
    bDescriptorType=0,
    bDescriptorSubtype=0,
    bTerminalId=3,
    wTerminalType=0x0101, # USB streaming
    bAssocTerminal=0,
    bSourceID=2,
    bCSourceID=1, # Clock
    bmControls=0x0, # D[ 1: 0] = Copy protect control
                    # D[ 3: 2] = Connector control
                    # D[ 5: 4] = Overload control
                    # D[ 7: 6] = Underflow control
                    # D[ 9: 8] = Overflow control
                    # TODO: See if any feature is applicable
    iTerminal=12
)
config0_interface3_alt0 = InterfaceDescriptor( # Audio AS, alt 0 (no endpoint)
    bLength=0,
    bDescriptorType=0,
    bInterfaceNumber=1 + (2 if HAS_CDC_ACM else 0),
    bAlternateSetting=0,
    bNumEndpoints=0,
    bInterfaceClass=0x1, # Audio
    bInterfaceSubClass=0x2, # AUDIOSTREAMING
    bInterfaceProtocol=0x20, # 2.0
    iInterface=13 # String descriptor number
)
config0_interface3_alt1 = InterfaceDescriptor( # Audio AS, alt 1
    bLength=0,
    bDescriptorType=0,
    bInterfaceNumber=1 + (2 if HAS_CDC_ACM else 0),
    bAlternateSetting=1,
    bNumEndpoints=1,
    bInterfaceClass=0x1, # Audio
    bInterfaceSubClass=0x2, # AUDIOSTREAMING
    bInterfaceProtocol=0x20, # 2.0
    iInterface=14 # String descriptor number
)
config0_audio_ac = AudioASInterfaceDescriptor(
    bLength=0,
    bDescriptorType=0,
    bDescriptorSubtype=0,
    bTerminalLink=3, # The output terminal
    bmControls=0x0,
    bFormatType=0x1, # Type I (cf. Audio Data Formats 1.0)
    bmFormats=0x1, # TODO verify UAC2
                   # D0 = PCM
                   # D1 = PCM8
                   # D2 = IEEE_FLOAT
                   # D3 = ALAW
                   # D4 = MULAW
                   # D5 = RAW_DATA
    bNrChannels=2,
    bmChannelConfig=0x3,
    iChannelNames=0
)
config0_audio_typeI = AudioTypeIDescriptor(
    bLength=0,
    bDescriptorType=0,
    bDescriptorSubtype=0,
    bFormatType=0,
    bSubslotSize=4, # 32b
    bBitResolution=24 # 24b
)
# https://github.com/MicrosoftDocs/windows-driver-docs/blob/staging/windows-driver-docs-pr/audio/usb-2-0-audio-drivers.md
n_bytes_per_frame = 8 # 2 channels * 32b
# Amount of frames per millisecond (SOF)
n_frames = AUDIO_SAMPLING_FREQUENCY_HZ // 1000 + 1 # Add ceiling + jitter frame,
# according to usbaudio2 for FMT 2.0 Section 2.3.3.1

config0_interface3_endpoint0 = EndpointDescriptor(
    bLength=0,
    bDescriptorType=0,
    bEndpointAddress=0x83, # IN, address=3
    bmAttributes=0x05, # Isochronous endpoint; asynchronous; data EP
    wMaxPacketSize=n_bytes_per_frame * n_frames,
    bInterval=1
)

config0_interface3_endpoint0_as = AudioASIsochronousDataEndpointDescriptor(
    bLength=0,
    bDescriptorType=0,
    bDescriptorSubtype=0,
    bmAttributes=0x0, # D7 (MaxPacketsOnly) ignored on Windows
    bmControls=0x0, # Ignored on Windows
    bLockDelayUnits=0x0, # Ignored on Windows
    wLockDelay=0 # Ignored on Windows
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
        # Config0.IAD
        "Audio",
        # Config0.Interface2
        "Audio control",
        # Config0.audio.clock
        "Audio clock source",
        # Config0.audio.input_terminal
        "Audio input terminal",
        # Config0.audio.output_terminal
        "Audio output terminal",
        # Config0.Interface3_alt0
        "Audio streaming alt. 0",
        # Config0.Interface3_alt1
        "Audio streaming alt. 1",
        # Config0.audio.as
        "Audio stream",
    ]
)

assembled_audio_ac = assemble_audio_ac_descriptor(
    config0_audio_control_interface_header,
    [
        config0_audio_clock_source, config0_audio_input_terminal,
        config0_audio_output_terminal
    ]
)

config0_descriptors = list()
if HAS_CDC_ACM:
    config0_descriptors += [
         config0_iad_cdc_acm,
         config0_interface0,
         config0_cdc_header_functional, config0_cdc_union_interface_functional,
         config0_cdc_call_management_functional, config0_cdc_acm_functional,
         config0_interface0_endpoint0,
         config0_interface1, config0_interface1_endpoint0,
            config0_interface1_endpoint1,
    ]
config0_descriptors += [
     config0_iad_audio,
     config0_interface2,
     assembled_audio_ac,
     config0_interface3_alt0,
     config0_interface3_alt1,
     config0_audio_ac, config0_audio_typeI,
     config0_interface3_endpoint0, config0_interface3_endpoint0_as
]

assembled_config0 = assemble_configuration_descriptor(config0, config0_descriptors)
