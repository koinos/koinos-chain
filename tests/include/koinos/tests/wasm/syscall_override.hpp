unsigned char syscall_override_wasm[] = {
  0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, 0x01, 0x0c, 0x02, 0x60,
  0x05, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x00, 0x60, 0x00, 0x00, 0x02, 0x2d,
  0x02, 0x03, 0x65, 0x6e, 0x76, 0x12, 0x69, 0x6e, 0x76, 0x6f, 0x6b, 0x65,
  0x5f, 0x73, 0x79, 0x73, 0x74, 0x65, 0x6d, 0x5f, 0x63, 0x61, 0x6c, 0x6c,
  0x00, 0x00, 0x03, 0x65, 0x6e, 0x76, 0x0c, 0x69, 0x6e, 0x76, 0x6f, 0x6b,
  0x65, 0x5f, 0x74, 0x68, 0x75, 0x6e, 0x6b, 0x00, 0x00, 0x03, 0x02, 0x01,
  0x01, 0x05, 0x03, 0x01, 0x00, 0x02, 0x06, 0x08, 0x01, 0x7f, 0x01, 0x41,
  0x90, 0x88, 0x04, 0x0b, 0x07, 0x13, 0x02, 0x06, 0x6d, 0x65, 0x6d, 0x6f,
  0x72, 0x79, 0x02, 0x00, 0x06, 0x5f, 0x73, 0x74, 0x61, 0x72, 0x74, 0x00,
  0x02, 0x0a, 0xf3, 0x03, 0x01, 0xf0, 0x03, 0x01, 0x06, 0x7f, 0x23, 0x00,
  0x41, 0xe0, 0x01, 0x6b, 0x22, 0x02, 0x24, 0x00, 0x41, 0x0f, 0x20, 0x02,
  0x41, 0x00, 0x3a, 0x00, 0x00, 0x20, 0x02, 0x41, 0xc1, 0x00, 0x6a, 0x22,
  0x00, 0x41, 0x01, 0x6b, 0x41, 0x00, 0x3a, 0x00, 0x00, 0x20, 0x02, 0x41,
  0x00, 0x3a, 0x00, 0x02, 0x20, 0x02, 0x41, 0x00, 0x3a, 0x00, 0x01, 0x20,
  0x00, 0x41, 0x03, 0x6b, 0x41, 0x00, 0x3a, 0x00, 0x00, 0x20, 0x00, 0x41,
  0x02, 0x6b, 0x41, 0x00, 0x3a, 0x00, 0x00, 0x20, 0x02, 0x41, 0x00, 0x3a,
  0x00, 0x03, 0x20, 0x00, 0x41, 0x04, 0x6b, 0x41, 0x00, 0x3a, 0x00, 0x00,
  0x20, 0x02, 0x41, 0x00, 0x20, 0x02, 0x6b, 0x41, 0x03, 0x71, 0x22, 0x01,
  0x6a, 0x22, 0x00, 0x41, 0x00, 0x36, 0x02, 0x00, 0x20, 0x00, 0x41, 0xc1,
  0x00, 0x20, 0x01, 0x6b, 0x41, 0x7c, 0x71, 0x22, 0x03, 0x6a, 0x22, 0x01,
  0x41, 0x04, 0x6b, 0x41, 0x00, 0x36, 0x02, 0x00, 0x02, 0x40, 0x20, 0x03,
  0x41, 0x09, 0x49, 0x0d, 0x00, 0x20, 0x00, 0x41, 0x00, 0x36, 0x02, 0x08,
  0x20, 0x00, 0x41, 0x00, 0x36, 0x02, 0x04, 0x20, 0x01, 0x41, 0x08, 0x6b,
  0x41, 0x00, 0x36, 0x02, 0x00, 0x20, 0x01, 0x41, 0x0c, 0x6b, 0x41, 0x00,
  0x36, 0x02, 0x00, 0x20, 0x03, 0x41, 0x19, 0x49, 0x0d, 0x00, 0x20, 0x00,
  0x41, 0x00, 0x36, 0x02, 0x18, 0x20, 0x00, 0x41, 0x00, 0x36, 0x02, 0x14,
  0x20, 0x00, 0x41, 0x00, 0x36, 0x02, 0x10, 0x20, 0x00, 0x41, 0x00, 0x36,
  0x02, 0x0c, 0x20, 0x01, 0x41, 0x10, 0x6b, 0x41, 0x00, 0x36, 0x02, 0x00,
  0x20, 0x01, 0x41, 0x14, 0x6b, 0x41, 0x00, 0x36, 0x02, 0x00, 0x20, 0x01,
  0x41, 0x18, 0x6b, 0x41, 0x00, 0x36, 0x02, 0x00, 0x20, 0x01, 0x41, 0x1c,
  0x6b, 0x41, 0x00, 0x36, 0x02, 0x00, 0x20, 0x03, 0x20, 0x00, 0x41, 0x04,
  0x71, 0x41, 0x18, 0x72, 0x22, 0x03, 0x6b, 0x22, 0x01, 0x41, 0x20, 0x49,
  0x0d, 0x00, 0x20, 0x00, 0x20, 0x03, 0x6a, 0x21, 0x00, 0x03, 0x40, 0x20,
  0x00, 0x42, 0x00, 0x37, 0x03, 0x00, 0x20, 0x00, 0x41, 0x18, 0x6a, 0x42,
  0x00, 0x37, 0x03, 0x00, 0x20, 0x00, 0x41, 0x10, 0x6a, 0x42, 0x00, 0x37,
  0x03, 0x00, 0x20, 0x00, 0x41, 0x08, 0x6a, 0x42, 0x00, 0x37, 0x03, 0x00,
  0x20, 0x00, 0x41, 0x20, 0x6a, 0x21, 0x00, 0x20, 0x01, 0x41, 0x20, 0x6b,
  0x22, 0x01, 0x41, 0x1f, 0x4b, 0x0d, 0x00, 0x0b, 0x0b, 0x20, 0x02, 0x22,
  0x00, 0x41, 0x3f, 0x41, 0x00, 0x41, 0x00, 0x10, 0x00, 0x20, 0x00, 0x41,
  0x0a, 0x3a, 0x00, 0x50, 0x20, 0x00, 0x41, 0x80, 0x08, 0x28, 0x00, 0x00,
  0x36, 0x01, 0x52, 0x20, 0x00, 0x41, 0x84, 0x08, 0x2f, 0x00, 0x00, 0x3b,
  0x01, 0x56, 0x02, 0x40, 0x20, 0x00, 0x2d, 0x00, 0x04, 0x22, 0x01, 0x45,
  0x0d, 0x00, 0x20, 0x00, 0x41, 0xd0, 0x00, 0x6a, 0x41, 0x08, 0x72, 0x21,
  0x03, 0x20, 0x00, 0x41, 0x05, 0x72, 0x21, 0x05, 0x41, 0x00, 0x21, 0x02,
  0x03, 0x40, 0x20, 0x02, 0x20, 0x03, 0x6a, 0x20, 0x01, 0x3a, 0x00, 0x00,
  0x20, 0x02, 0x41, 0x01, 0x6a, 0x21, 0x04, 0x20, 0x02, 0x41, 0xff, 0x00,
  0x4b, 0x0d, 0x01, 0x20, 0x02, 0x20, 0x05, 0x6a, 0x20, 0x04, 0x21, 0x02,
  0x2d, 0x00, 0x00, 0x22, 0x01, 0x0d, 0x00, 0x0b, 0x0b, 0x20, 0x00, 0x20,
  0x04, 0x41, 0x06, 0x6a, 0x3a, 0x00, 0x51, 0x41, 0x01, 0x41, 0x00, 0x41,
  0x00, 0x20, 0x00, 0x41, 0xd0, 0x00, 0x6a, 0x20, 0x04, 0x41, 0x08, 0x6a,
  0x10, 0x01, 0x20, 0x00, 0x41, 0xe0, 0x01, 0x6a, 0x24, 0x00, 0x0b, 0x0b,
  0x0d, 0x01, 0x00, 0x41, 0x80, 0x08, 0x0b, 0x06, 0x74, 0x65, 0x73, 0x74,
  0x3a, 0x20, 0x00, 0x76, 0x09, 0x70, 0x72, 0x6f, 0x64, 0x75, 0x63, 0x65,
  0x72, 0x73, 0x01, 0x0c, 0x70, 0x72, 0x6f, 0x63, 0x65, 0x73, 0x73, 0x65,
  0x64, 0x2d, 0x62, 0x79, 0x01, 0x05, 0x63, 0x6c, 0x61, 0x6e, 0x67, 0x56,
  0x31, 0x31, 0x2e, 0x30, 0x2e, 0x30, 0x20, 0x28, 0x68, 0x74, 0x74, 0x70,
  0x73, 0x3a, 0x2f, 0x2f, 0x67, 0x69, 0x74, 0x68, 0x75, 0x62, 0x2e, 0x63,
  0x6f, 0x6d, 0x2f, 0x6c, 0x6c, 0x76, 0x6d, 0x2f, 0x6c, 0x6c, 0x76, 0x6d,
  0x2d, 0x70, 0x72, 0x6f, 0x6a, 0x65, 0x63, 0x74, 0x20, 0x31, 0x37, 0x36,
  0x32, 0x34, 0x39, 0x62, 0x64, 0x36, 0x37, 0x33, 0x32, 0x61, 0x38, 0x30,
  0x34, 0x34, 0x64, 0x34, 0x35, 0x37, 0x30, 0x39, 0x32, 0x65, 0x64, 0x39,
  0x33, 0x32, 0x37, 0x36, 0x38, 0x37, 0x32, 0x34, 0x61, 0x36, 0x66, 0x30,
  0x36, 0x29
};
unsigned int syscall_override_wasm_len = 746;
