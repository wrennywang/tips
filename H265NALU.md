# h.265与h.264一样，以0x00 00 00 01或0x00 00 01为前缀码
# 相比h.264多了一些概念，例如VPS
# nalu语法元素
- h.264, 1字节nalu语法元素
forbidden_bit(1bits), nal_reference_bit(2bits), nal_unit_type(5bits)
nalu_type = first_byte_in_nal & 0x1F

| nalu_type | h.264语法结构类型 |
| --- | --- |
| 01 | P/B/I Slice |
| 05 | I Slice |
| 06 | SEI |
| 07 | SPS |
| 08 | PPS |

- h.265，2字节nalu语法元素

| nal_unit_header() { | Descriptor |
| ---| ---|
| forbidden_zero_bit | f(1) |
| nal_unit_type | u(6) |
| nuh_layer_id | u(6) |
| nuh_temporal_id_plus1 | u(3) |

nalu_type = first_byte_in_nal & 0x7E >> 1;

| nalu_type | name |
| --- | --- |
| 0 | TRAIL_N |
| 1 | TRAIL_R |
| 19 | IDR_W_RADL |
| 32 | VPS_NUT |
| 33 | SPS_NUT |
| 34 | PPS_NUT |

示例
> 00 00 00 01 40 01 ---> (0x40 & 0x7E) >> 1 = 32 ---> VPS

> 00 00 00 01 42 01 ---> (0x42 & 0x7E) >> 1 = 33 ---> SPS

> 00 00 00 01 44 01 ---> (0x44 & 0x7E) >> 1 = 34 ---> PPS

> 00 00 00 01 26 01 ---> (0x26 & 0x7E) >> 1 = 19 ---> IDR
