void print_byte(char b, size_t s) {
  for(int i =0; i < s; i++) {
    Serial.printf("%d", !!((b << i) & 0x80));
  }
  Serial.printf("\n");
}

void print_array(bool* a, size_t s) {
  for(int i = 0; i < s; i++) {
    Serial.printf("%d", a[i]);
  }

  Serial.printf("\n");
}

char pack(bool* byte_as_array, size_t s) {
  char byte = 0;
  for(int i = 0; i < s; i++) {
    if(byte_as_array[7 - i]) byte |= 1UL << i;
  }

  return byte;
}

bool* unpack(char byte, size_t s) {
  int i;
  bool* arr;
  i = 0;
  arr = (bool*)malloc(sizeof(bool) * s);
  memset(arr, 0, sizeof(arr));
  while(i < s) {
      arr[(s - 1) - i] = byte & 0x01;

      byte = byte >> 1; 
      i++;
  }

  return arr;
}