# stb image codecs

Vendored unchanged from https://github.com/nothings/stb at commit
`003501f6d7a8668ce8b6d875e34dd97ad0d5d5b5` using HTTPS:

- `stb_image.h` v2.30: PNG/JPEG/BMP decoding (other formats disabled in our wrapper).
- `stb_image_write.h` v1.16: grayscale PNG encoding.
- `LICENSE`: upstream MIT/public-domain dual license; retained verbatim.

SHA256 checksums of the downloaded files:

```text
594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3  stb_image.h
cbd5f0ad7a9cf4468affb36354a1d2338034f2c12473cf1a8e32053cb6914a05  stb_image_write.h
bebfe904b14301657e4e5d655c811d51fd31b97c455b9cc2d8600d6bac6cff63  LICENSE
```

The implementation macros live only in `src/image_io.cpp`. Building the project
does not download packages or require an installed image framework.
