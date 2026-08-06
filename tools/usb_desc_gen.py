import sys
import math

import usb_desc

def assemble_configuration_descriptor(
    configuration_descriptor,
    interface_endpoint_descriptors):
    configuration_descriptor.wTotalLength = \
        9 + sum(len(i.to_bytes()) for i in interface_endpoint_descriptors)
    return bytes().join(i.to_bytes() for i in [configuration_descriptor] + interface_endpoint_descriptors)

def u16_to_bytes(x):
    return bytes([x & 0xff, (x >> 8) & 0xff])

def to_c(constant_name, raw_data, static=False):
    if static:
        ret = "static "
    else:
        ret = ""
    ret += "const uint8_t __attribute__((aligned(4))) " + str(constant_name) + "[" \
        + str(len(raw_data)) + "] = {\n"
    for i in range(0, len(raw_data), 8):
        ret += "    " + " ".join(hex(x) + ("" if i+j+1 == len(raw_data) else ",") for j, x in enumerate(raw_data[i : i+8])) + "\n"
    ret += "};\n"
    return ret

def to_h(typename, constant_name, n_data):
    return "extern " + typename + " const " + str(constant_name) + "[" \
        + str(n_data) + "];\n"

def generate_string_index(constant_name, string_names):
    ret = "const uint8_t * const " + constant_name + "[" \
        + str(len(string_names)) + "] = {\n"
    ret += "\n".join("    " + name + ("" if i+1 == len(string_names) else ",") for i, name in enumerate(string_names))
    ret += "\n};\n"
    return ret

def header(guard, lines):
    return "\n".join([
            "#ifndef " + str(guard),
            "#define " + str(guard),
            "#include <stdint.h>"
        ] + lines + [
            "#endif // " + str(guard)
        ])

basename = str(sys.argv[1])

audio_samples_remainder = usb_desc.AUDIO_SAMPLING_FREQUENCY_HZ % 1000 # Per millisecond
# R * short_packets = (1000 - R) * long_packets
audio_packet_gcd = math.gcd(audio_samples_remainder, 1000)
coeff_short_packets = audio_samples_remainder // audio_packet_gcd
coeff_long_packets = (1000 - audio_samples_remainder) // audio_packet_gcd
# Solve the equation
audio_short_packets = coeff_long_packets
audio_long_packets = coeff_short_packets

string_names = list("board_usb_string" + str(i) + "_descriptor" for i in range(len(usb_desc.strings)))
with open(basename + ".c", "w") as f_c, \
    open(basename + ".h", "w") as f_h:
    f_c.write("#include <stdint.h>\n")
    f_c.write(to_c("board_usb_device_descriptor", usb_desc.device.to_bytes()))
    f_c.write(to_c("board_usb_configuration0_descriptor", usb_desc.assembled_config0))

    for name, descriptor in zip(string_names, usb_desc.strings):
        f_c.write(to_c(name, descriptor.to_bytes(), True))
    f_c.write(generate_string_index("board_usb_string_descriptors", string_names))

    header_body = ("#define HAS_CDC_ACM\n" if usb_desc.HAS_CDC_ACM else "") \
        + "#define AUDIO_SAMPLING_FREQUENCY_HZ " \
        + str(usb_desc.AUDIO_SAMPLING_FREQUENCY_HZ) + "\n" \
        + "#define AUDIO_AMOUNT_SHORT_PACKETS " + str(audio_short_packets) + "\n" \
        + "#define AUDIO_AMOUNT_LONG_PACKETS " + str(audio_long_packets) + "\n" \
        + to_h("uint8_t", "board_usb_device_descriptor", len(usb_desc.device.to_bytes())) \
        + to_h("uint8_t", "board_usb_configuration0_descriptor", len(usb_desc.assembled_config0)) \
        + to_h("const uint8_t *", "board_usb_string_descriptors", len(string_names))
    f_h.write(header("BOARD_USB_DESCRIPTORS_H", header_body.split("\n")))
