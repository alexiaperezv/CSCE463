/* Checksum class declaration is included in this file
* CSCE 463-500, Spring 2022
* By Alexia Perez
*/

#pragma once

class Checksum {
public:
	Checksum();
	DWORD CRC32(unsigned char* buf, size_t len);
	DWORD crc_table[256];
};