/**
 * Copyright 2014 Andrea Farruggia
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * 		http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
*/

#ifndef __COPY_ROU_
#define __COPY_ROU_


#if defined(__i386__) || defined(__x86_64__) || defined(__powerpc__)

#define UNALIGNED_LOAD16(_p) (*reinterpret_cast<const uint16_t *>(_p))
#define UNALIGNED_LOAD32(_p) (*reinterpret_cast<const uint32_t *>(_p))
#define UNALIGNED_LOAD64(_p) (*reinterpret_cast<const uint64_t *>(_p))

#define UNALIGNED_STORE16(_p, _val) (*reinterpret_cast<uint16_t *>(_p) = (_val))
#define UNALIGNED_STORE32(_p, _val) (*reinterpret_cast<uint32_t *>(_p) = (_val))
#define UNALIGNED_STORE64(_p, _val) (*reinterpret_cast<uint64_t *>(_p) = (_val))

#else

// These functions are provided for architectures that don't support
// unaligned loads and stores.
inline uint16_t UNALIGNED_LOAD16(const void *p) {
  uint16_t t;
  memcpy(&t, p, sizeof t);
  return t;
}

inline uint32_t UNALIGNED_LOAD32(const void *p) {
  uint32_t t;
  memcpy(&t, p, sizeof t);
  return t;
}

inline uint64_t UNALIGNED_LOAD64(const void *p) {
  uint64_t t;
  memcpy(&t, p, sizeof t);
  return t;
}

inline void UNALIGNED_STORE16(void *p, uint16_t v) {
  memcpy(p, &v, sizeof v);
}

inline void UNALIGNED_STORE32(void *p, uint32_t v) {
  memcpy(p, &v, sizeof v);
}

inline void UNALIGNED_STORE64(void *p, uint64_t v) {
  memcpy(p, &v, sizeof v);
}
#endif


template<typename T>
inline void copy_fast(T *op, T* src, int len)
{
	while (op - src < 8) {
		UNALIGNED_STORE64(op, UNALIGNED_LOAD64(src));
		len -= op - src;
		op += op - src;
	}
	while (len > 0) {
		UNALIGNED_STORE64(op, UNALIGNED_LOAD64(src));
		src += 8;
		op += 8;
		len -= 8;
	}
}

template <typename T>
inline void u_copy_fast(T *dest, T *src, int len)
{
	while (len > 0) {
		UNALIGNED_STORE64(dest, UNALIGNED_LOAD64(src));
		src += 8;
		dest	+= 8;
		len -= 8;
	}
}

template <typename T>
inline void copy_mem(T *dst, T *src, size_t len) {
    T *end = src + len;
    while (src < end)
        *dst++ = *src++;
}

#endif
