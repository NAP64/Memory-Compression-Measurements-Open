/*

    CPACK Compression Code

    Transplanted to C from github repository:
	https://github.com/benschreiber/cpack
	Bases on paper:
  	http://ieeexplore.ieee.org/document/5229354/

	Original code is under MIT licence
    
*/

#include <stdint.h>
#include <assert.h>
#include <stdbool.h>

static void set_bit1(uint8_t* output, int* idx, bool a)
{
	if(a == 0) {
		output[*idx/8] &= ~(1 << (*idx%8));
	} else {
		output[*idx/8] |= 1 << (*idx%8);
	}

	*idx = *idx + 1;
}

static void set_bit2(uint8_t* output, int* idx, bool a, bool b)
{
	set_bit1(output, idx, a);
	set_bit1(output, idx, b);
}

static void set_bit(uint8_t* output, int* idx, bool a, bool b, bool c, bool d)
{
	set_bit1(output, idx, a);
	set_bit1(output, idx, b);
	set_bit1(output, idx, c);
	set_bit1(output, idx, d);
}

static void set_byte(uint8_t* output, int* idx, uint8_t byte)
{
	set_bit1(output, idx, byte & 128);
	set_bit1(output, idx, byte & 64);
	set_bit1(output, idx, byte & 32);
	set_bit1(output, idx, byte & 16);
	set_bit1(output, idx, byte & 8);
	set_bit1(output, idx, byte & 4);
	set_bit1(output, idx, byte & 2);
	set_bit1(output, idx, byte & 1);
}

static void set_idx(uint8_t* output, int* out_idx, int idx)
{
	set_bit1(output, out_idx, idx & 8);
	set_bit1(output, out_idx, idx & 4);
	set_bit1(output, out_idx, idx & 2);
	set_bit1(output, out_idx, idx & 1);
}

// returns size of compressed line in bytes
int cpack_compress(uint8_t * input, uint8_t * output)
{
	int i, j, init = 0;
	int* out_idx = &init;
	uint8_t dict[64] = {0};
	int dict_size = 0;

	for(i = 0; i < 16; i++) {
		assert(*out_idx < 544);

		bool found = false;

		uint8_t a = input[4*i];
		uint8_t b = input[4*i+1];
		uint8_t c = input[4*i+2];
		uint8_t d = input[4*i+3];

		// check against zero patterns
		if(a == 0 && b == 0 && c == 0) {
			if (d == 0) { // pattern zzzz ouput (00)
				set_bit2(output, out_idx, 0, 0);
			} else { // pattern zzzx output (1101)B
				set_bit(output, out_idx, 1, 1, 0, 1);
				set_byte(output, out_idx, d);
			}
			continue;
		}

		// check against dictionary
		for(j = 0; j < dict_size; j++) {	
			if(a == dict[4*j] && b == dict[4*j+1]) {
				if(c == dict[4*j+2]) {
					if(d == dict[4*j+3]) { // pattern mmmm output (10) bbbb
						set_bit2(output, out_idx, 1, 0);
						set_idx(output, out_idx, j);
					} else { // pattern mmmx output (1110)bbbbB
						set_bit(output, out_idx, 1, 1, 1, 0);
						set_idx(output, out_idx, j);
						set_byte(output, out_idx, d);
					}
				} else { // pattern mmxx output (1100)bbbbBB
					set_bit(output, out_idx, 1, 1, 0, 0);
					set_idx(output, out_idx, j);
					set_byte(output, out_idx, c);
					set_byte(output, out_idx, d);
				}
				found = true;
				break;
			}
		}

		if(found) {
			continue;
		}

		// pattern xxxx output (01)BBBB
		set_bit2(output, out_idx, 0, 1);
		set_byte(output, out_idx, a);
		set_byte(output, out_idx, b);
		set_byte(output, out_idx, c);
		set_byte(output, out_idx, d);

		// add new pattern to dictionary
		dict[4*dict_size] = a;
		dict[4*dict_size+1] = b;
		dict[4*dict_size+2] = c;
		dict[4*dict_size+3] = d;

		dict_size += 1;
	}

	return *out_idx;
}

static uint8_t read_bit(const uint8_t* input, int* i)
{
	if (input[(*i)/8] & (1 << ((*i)%8))) {
		(*i)++;
		return 1;
	} else {
		(*i)++;
		return 0;
	}
}

static uint8_t read_byte(const uint8_t* input, int* i)
{
	uint8_t byte = 0;
	byte += 128*read_bit(input, i);
	byte +=  64*read_bit(input, i);
	byte +=  32*read_bit(input, i);
	byte +=  16*read_bit(input, i);
	byte +=   8*read_bit(input, i);
	byte +=   4*read_bit(input, i);
	byte +=   2*read_bit(input, i);
	byte +=   1*read_bit(input, i);

	return byte;
}

static int read_idx(const uint8_t* input, int* i)
{
	int idx = 0;
	idx += 8*read_bit(input, i);
	idx += 4*read_bit(input, i);
	idx += 2*read_bit(input, i);
	idx += 1*read_bit(input, i);

	return idx;
}

// returns 0. could be modified to return 1 on error.
int cpack_decompress(const uint8_t input[68], uint8_t output[64])
{
	uint8_t dict[64];
	int dict_size = 0;
	int out_idx = 0;
    int j;
	for(j = 0; out_idx < 64; /**/) {
		int* i = &j;
		bool bit1 = read_bit(input, i);
		bool bit2 = read_bit(input, i);

		if(!bit1 && !bit2) { // (00) zzzz
			output[out_idx++] = 0;
			output[out_idx++] = 0;
			output[out_idx++] = 0;
			output[out_idx++] = 0;
		} else if(!bit1 && bit2) { // (01) xxxx
			uint8_t a = read_byte(input, i);
			uint8_t b = read_byte(input, i);
			uint8_t c = read_byte(input, i);
			uint8_t d = read_byte(input, i);
			
			dict[dict_size++] = a;
			dict[dict_size++] = b;
			dict[dict_size++] = c;
			dict[dict_size++] = d;

			output[out_idx++] = a;
			output[out_idx++] = b;
			output[out_idx++] = c;
			output[out_idx++] = d;
		} else if(bit1 && !bit2) { // (10) mmmm
			int idx = read_idx(input, i); 

			output[out_idx++] = dict[4*idx];
			output[out_idx++] = dict[4*idx+1];
			output[out_idx++] = dict[4*idx+2];
			output[out_idx++] = dict[4*idx+3];
		} else {
			bool bit3 = read_bit(input, i);
			bool bit4 = read_bit(input, i);

			if(!bit3 && !bit4) { // (1100) mmxx
				int idx = read_idx(input, i);
				uint8_t c = read_byte(input, i);
				uint8_t d = read_byte(input, i);

				output[out_idx++] = dict[4*idx];
				output[out_idx++] = dict[4*idx+1];
				output[out_idx++] = c;
				output[out_idx++] = d;
			} else if(!bit3 && bit4) { // (1101) zzzx
				uint8_t d = read_byte(input, i);
				
				output[out_idx++] = 0;
				output[out_idx++] = 0;
				output[out_idx++] = 0;
				output[out_idx++] = d;
			} else { // (1110) mmmx
				int idx = read_idx(input, i);
				uint8_t d = read_byte(input, i);

				output[out_idx++] = dict[4*idx];
				output[out_idx++] = dict[4*idx+1];
				output[out_idx++] = dict[4*idx+2];
				output[out_idx++] = d;
			}
		}
	}

	return 0;
}