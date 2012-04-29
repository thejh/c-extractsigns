#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "lib/cNBT/nbt.h"
#include "lib/cNBT/list.h"
#include <string.h>
#include <netinet/in.h>

#define INDEX_BYTES 4096
#define COLUMN_GRID_SIZE 4096
#define NUMBER_OF_COLUMNS 32 * 32

#define OK 0
#define ERR -1

/*
 * How many chunks do we have for that column?
 */
static int how_many_chunks(unsigned char *raw_column_list, int column) {
	return raw_column_list[column*4+3];
}

static void seek_to_column(FILE *file, unsigned char *raw_column_list, int i) {
	i *= 4;
	long a = raw_column_list[i];
	long b = raw_column_list[i+1];
	long c = raw_column_list[i+2];
	//printf("seek_to_column uses bytes: %lx %lx %lx\n", a, b, c);
	long pos = ((a << 16) + (b << 8) + c);
	//printf("concatenated: %lx\n", pos);
	pos *= COLUMN_GRID_SIZE;
	//printf("seeking to %li\n", pos);
	fseek(file, pos, SEEK_SET);
}

/*
 * WARNING: This methods malloc()s RAM!
 */
static char* read_bytes_from_file(FILE *file, size_t length) {
	char *buf = malloc(length);
	assert(fread(buf, length, 1, file) == 1);
	return buf;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		puts("FAIL: Please specify exactly one argument!");
		return 1;
	}
	
	char *path = argv[1];
	FILE *file = fopen(path, "r");
	char *raw_column_list = malloc(INDEX_BYTES);
	assert(fread(raw_column_list, 1, INDEX_BYTES, file) == INDEX_BYTES);
	//printf("First four bytes of the file: %hhx, %hhx, %hhx, %hhx\n", raw_column_list[0], raw_column_list[1], raw_column_list[2], raw_column_list[3]);
	
	int column;
	for (column = 0; column < NUMBER_OF_COLUMNS; column++) {
		//printf("processing column %i...\n", column);
		int number_of_chunks = how_many_chunks(raw_column_list, column);
		if (number_of_chunks != 0) {
			seek_to_column(file, raw_column_list, column);
			unsigned char _column_data_size[4];
			size_t fread_result = fread(_column_data_size, 4, 1, file);
			if (fread_result != 1) {
			  printf("expected 1, got %i, error: %i\n", (int) fread_result, ferror(file));
			  assert(0); // fread result is bad
			}
			uint32_t column_data_size = (_column_data_size[0] << 24) + (_column_data_size[1] << 16) + (_column_data_size[2] << 8) + _column_data_size[3];
			//printf("column_data_size: %x\n", column_data_size);
			//column_data_size++; // count the compression type byte as data, too
			char *column_data = read_bytes_from_file(file, (size_t)column_data_size);
			nbt_node* column_node = nbt_parse_compressed(column_data+1, (size_t)column_data_size);
			free(column_data);
			assert(column_node != NULL);
			
			
			assert(column_node->type == TAG_COMPOUND);
			const struct list_head* pos;
			nbt_node* level_compound;
			list_for_each(pos, &column_node->payload.tag_compound->entry) {
				level_compound = list_entry(pos, const struct nbt_list, entry)->data;
			}
			assert(level_compound != NULL);
			
			assert(level_compound->type == TAG_COMPOUND);
			nbt_node* tile_entities_list = NULL;
			nbt_node* tile_entities_list_tmp;
			list_for_each(pos, &level_compound->payload.tag_compound->entry) {
				tile_entities_list_tmp = list_entry(pos, const struct nbt_list, entry)->data;
				if (strcmp(tile_entities_list_tmp->name, "TileEntities") == 0) {
					assert(tile_entities_list == NULL);
					tile_entities_list = tile_entities_list_tmp;
				}
			}
			assert(tile_entities_list != NULL);
			
			assert(tile_entities_list->type == TAG_LIST);
			nbt_node* tile_entity;
			list_for_each(pos, &tile_entities_list->payload.tag_list->entry) {
				//puts("");
				tile_entity = list_entry(pos, const struct nbt_list, entry)->data;
				assert(tile_entity->type == TAG_COMPOUND);
				int is_wanted_tile_entity = 0;
				const struct list_head* pos2;
				nbt_node* tile_entity_property;
				int32_t x = 0;
				int32_t y = 0;
				int32_t z = 0;
				char *text[4]; // TODO init to NULL and check afterwards
				list_for_each(pos2, &tile_entity->payload.tag_compound->entry) {
					tile_entity_property = list_entry(pos2, const struct nbt_list, entry)->data;
					//printf("                             attrib: %s\n", tile_entity_property->name);
					if (tile_entity_property->type == TAG_STRING) {
						if (strcmp(tile_entity_property->name, "id") == 0) {
							is_wanted_tile_entity = strcmp(tile_entity_property->payload.tag_string, "Sign") == 0;
						} else if (strcmp(tile_entity_property->name, "Text1") == 0) {
							text[0] = tile_entity_property->payload.tag_string;
						} else if (strcmp(tile_entity_property->name, "Text2") == 0) {
							text[1] = tile_entity_property->payload.tag_string;
						} else if (strcmp(tile_entity_property->name, "Text3") == 0) {
							text[2] = tile_entity_property->payload.tag_string;
						} else if (strcmp(tile_entity_property->name, "Text4") == 0) {
							text[3] = tile_entity_property->payload.tag_string;
						}
					} else if (tile_entity_property->type == TAG_INT) {
						if (strcmp(tile_entity_property->name, "x") == 0) {
							x = tile_entity_property->payload.tag_int;
						} else if (strcmp(tile_entity_property->name, "y") == 0) {
							y = tile_entity_property->payload.tag_int;
						} else if (strcmp(tile_entity_property->name, "z") == 0) {
							z = tile_entity_property->payload.tag_int;
						}
					}
				}
				if (is_wanted_tile_entity) {
					printf("found a sign at (%i|%i|%i)\n", x, y, z);
					for (int i=0; i<4; i++) puts(text[i]);
					puts("");
				}
			}
			
			//char* str = nbt_dump_ascii(column_node);
			//puts(str);
			//free(str);
			
			nbt_free(column_node);
		}
	}
}
