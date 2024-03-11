// 
// FSLib is placed in the public domain.
// 
// Anyone is free to copy, modify, publish, use, compile, sell, or distribute this code,
// either in source code form or as a compiled binary, for any purpose,
// commercial or non - commercial, and by any means.
//

#ifndef __FSLIB_H__
#define __FSLIB_H__

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define LZMA_API_STATIC
#include <lzma.h>

#define ALPHABET_SIZE 29

#define NULL_NODE '#'
#define NULL_FILE '@'
#define HAS_FILE  '$'

typedef struct TRFILEDATA_S
{
	long pos;
	long size;
	long deflated;
#ifdef PACK_BIGFILE
	uint8_t* data;
#endif
} TRFILEDATA;

typedef struct TRFSNODE_S
{
	TRFILEDATA* file;
	struct TRFSNODE_S* children[ALPHABET_SIZE];
} TRFSNODE;

typedef struct TRFILESYS_S
{
	TRFSNODE* root;
	struct _iobuf* bigfile;
	long offset;
} TRFILESYS;

//
// INTERFACE
// 
// 
//#ifdef PACK_BIGFILE
//  TRFILESYS* create_trfs();
//  void insert_trfs(TRFILESYS* fs, const char* path, const char* vpath);
//  void write_trfs(const TRFILESYS* fs, const char* path);
//#endif
//
//  TRFILESYS* load_trfs(const char* path);
//  void unload_trfs(TRFILESYS** fs);
//  uint8_t* trfs_open(const TRFILESYS* fs, const char* path, size_t* file_size);
// 
//  How to pack files, example:
//     #define PACK_BIGFILE
//     #include <FSLib.h>
//     TRFILESYS* fs = create_trfs();
//     insert_trfs(fs, "D:\\png_bmw.png", "images\\car.png");
//     insert_trfs(fs, "D:\\png1.png", "images\\pngA.png");
//     insert_trfs(fs, "D:\\Free_Test_Data_2MB_OGG.ogg", "audio\\oggmb.ogg");
//     // ...
//     write_trfs(fs, "D:\\data.big");
//     unload_trfs(&fs); 
// 
//  How to read files, example:
//     TRFILESYS* fs_ld = load_trfs("D:\\data.big");
//     size_t file_size;
//     uint8_t* filebuf = trfs_open(fs_ld, "audio\\oggmb.ogg", &file_size);
//     //...
//     free(filebuf);
//

//
// IMPLEMENTATION
//

static TRFSNODE* create_trfs_node()
{
	TRFSNODE* node = (TRFSNODE*)calloc(1, sizeof(TRFSNODE));
	return node;
}

static char transform_char(char ch)
{
	if (ch == '.')
		ch = '[';
	if (ch == '_')
		ch = ']';
	return toupper(ch) - 'A';
}

static char inverse_char(char ch)
{
	ch = toupper(ch) + 'A';
	if (ch == '[')
		ch = '.';
	if (ch == ']')
		ch = '_';
	return ch;
}

TRFILESYS* create_trfs()
{
	TRFILESYS* fs = (TRFILESYS*)calloc(1, sizeof(TRFILESYS));
	if (fs != NULL)
		fs->root = create_trfs_node();
	return fs;
}

static uint8_t* compress(const uint8_t* const buffer, const size_t file_size, size_t* compressed_size)
{
	uint8_t* deflate = (uint8_t*)malloc(sizeof(uint8_t) * file_size * 2);
	if (deflate == NULL)
		return NULL;

	lzma_stream lzstrm = LZMA_STREAM_INIT;
	lzma_ret lzret = lzma_easy_encoder(&lzstrm, LZMA_PRESET_DEFAULT, LZMA_CHECK_CRC64);
	if (lzret != LZMA_OK)
	{
		free(deflate);
		return NULL;
	}

	lzstrm.next_in = buffer;
	lzstrm.avail_in = file_size;
	lzstrm.next_out = deflate;
	lzstrm.avail_out = file_size * 2;

	lzret = lzma_code(&lzstrm, LZMA_RUN);
	if (lzret != LZMA_OK && lzret != LZMA_STREAM_END)
	{
		free(deflate);
		return NULL;
	}

	lzma_code(&lzstrm, LZMA_FINISH);
	lzma_end(&lzstrm);

	*compressed_size = lzstrm.total_out;
	return deflate;
}

#ifdef PACK_BIGFILE
void insert_trfs(TRFILESYS* fs, const char* path, const char* vpath)
{
	if (path == NULL || vpath == NULL)
		return;

	TRFSNODE* node = fs->root;
	while (*vpath)
	{
		int idx = transform_char(*vpath);
		if (node->children[idx] == NULL)
			node->children[idx] = create_trfs_node();

		node = node->children[idx];
		vpath++;
	}

	FILE* fembed;
	fopen_s(&fembed, path, "rb");
	if (fembed == NULL)
		return;

	node->file = (TRFILEDATA*)calloc(1, sizeof(TRFILEDATA));
	if (node->file == NULL)
	{
		fclose(fembed);
		return;
	}

	fseek(fembed, 0, SEEK_END);
	node->file->size = ftell(fembed);

	rewind(fembed);

	uint8_t* filedata = (uint8_t*)malloc(sizeof(uint8_t) * node->file->size);
	if (filedata == NULL)
	{
		fclose(fembed);
		return;
	}

	size_t result = fread_s(filedata, node->file->size, node->file->size, 1, fembed);
	if (result != 1)
	{
		free(filedata);
		fclose(fembed);
		return;
	}

	fclose(fembed);

	size_t compressed_size;
	node->file->data = compress(filedata, node->file->size, &compressed_size);
	node->file->deflated = (long)compressed_size;
}

static void write_trie_node(FILE* ftbl, FILE* fdat, char value, const TRFSNODE* node)
{
	if (node == NULL)
	{
		fputc(NULL_NODE, ftbl);
		return;
	}

	fputc(value, ftbl);

	if (node->file == NULL)
		fputc(NULL_FILE, ftbl);
	else
	{
		static long pos = 0;

		node->file->pos = pos;
		fputc(HAS_FILE, ftbl);
		fwrite(&node->file->pos, sizeof(long), 1, ftbl);
		fwrite(&node->file->size, sizeof(long), 1, ftbl);
		fwrite(&node->file->deflated, sizeof(long), 1, ftbl);

		fwrite(node->file->data, node->file->size, 1, fdat);
		pos += node->file->size;
	}

	for (int i = 0; i < ALPHABET_SIZE; ++i)
		write_trie_node(ftbl, fdat, i, node->children[i]);
}

void write_trfs(const TRFILESYS* fs, const char* path)
{
	time_t t;
	srand((unsigned)time(&t));

	FILE* bigfile, * fdata;
	fopen_s(&bigfile, path, "wb");

	if (bigfile == NULL)
		return;

	size_t len = strlen(path);
	char* datadest = (char*)malloc(sizeof(char) * (len + 5));
	if (datadest == NULL)
		return;

	strcpy_s(datadest, len+5, path);
	strcat_s(datadest, len+5, ".tmp");
	fopen_s(&fdata, datadest, "wb+");

	if (fdata == NULL)
	{
		free(datadest);
		return;
	}

	write_trie_node(bigfile, fdata, '\0', fs->root);

	fflush(fdata);
	long size = ftell(fdata);

	uint8_t* data = (uint8_t*)malloc(sizeof(uint8_t) * size);
	if (data == NULL)
	{
		free(datadest);
		return;
	}

	rewind(fdata);

	size_t result = fread_s(data, size, size, 1, fdata);
	if (result != 1)
	{
		free(data);
		free(datadest);
		return;
	}

	fclose(fdata);

	fwrite(data, size, 1, bigfile);
	free(data);

	fflush(bigfile);
	fclose(bigfile);

	remove(datadest);
	free(datadest);
}
#endif //PACK_BIGFILE

static TRFSNODE* read_trie_node(FILE* ftbl)
{
	int ch = fgetc(ftbl);

	if (ch == NULL_NODE)
		return NULL;

	TRFSNODE* node = create_trfs_node();

	ch = fgetc(ftbl);
	if (ch == HAS_FILE)
	{
		long pos, size, deflated;
		fread_s(&pos, sizeof(long), sizeof(long), 1, ftbl);
		fread_s(&size, sizeof(long), sizeof(long), 1, ftbl);
		fread_s(&deflated, sizeof(long), sizeof(long), 1, ftbl);
		node->file = (TRFILEDATA*)calloc(1, sizeof(TRFILEDATA));
		if (node->file != NULL)
		{
			node->file->pos = pos;
			node->file->size = size;
			node->file->deflated = deflated;
		}
	}

	for (int idx = 0; idx < ALPHABET_SIZE; idx++)
		node->children[idx] = read_trie_node(ftbl);

	return node;
}

TRFILESYS* load_trfs(const char* path)
{
	TRFILESYS* fs = create_trfs();
	fopen_s(&fs->bigfile, path, "rb");

	if (fs->bigfile == NULL)
		return NULL;

	fs->root = read_trie_node(fs->bigfile);

	fs->offset = ftell(fs->bigfile);

	return fs;
}

static void unload_fsnode(TRFSNODE** node)
{
	if ((*node) == NULL)
		return;

	for (int i = 0; i < ALPHABET_SIZE; ++i)
		unload_fsnode(&(*node)->children[i]);

	if ((*node)->file != NULL)
	{
#ifdef PACK_BIGFILE
		if ((*node)->file->data != NULL)
			free((*node)->file->data);
#endif
		free((*node)->file);
		(*node)->file = NULL;
	}

	free(*node);
	(*node) = NULL;
}

void unload_trfs(TRFILESYS** fs)
{
	unload_fsnode(&(*fs)->root);
	if ((*fs)->bigfile != NULL)
		fclose((*fs)->bigfile);

	free(*fs);
	(*fs) = NULL;
}

static uint8_t* decompress(const uint8_t* deflate, const size_t deflate_size, const size_t file_size)
{
	uint8_t* decompressed = NULL;

	uint8_t* tmp = (uint8_t*)realloc(decompressed, file_size);
	if (tmp == NULL)
		return NULL;
	decompressed = tmp;

	lzma_stream lzstrm = LZMA_STREAM_INIT;
	lzma_ret ret = lzma_stream_decoder(&lzstrm, UINT64_MAX, LZMA_CONCATENATED);
	if (ret != LZMA_OK)
	{
		free(decompressed);
		return NULL;
	}

	lzstrm.next_in = deflate;
	lzstrm.avail_in = deflate_size;
	lzstrm.next_out = decompressed;
	lzstrm.avail_out = file_size;

	ret = lzma_code(&lzstrm, LZMA_FINISH);
	lzma_end(&lzstrm);

	if (ret != LZMA_STREAM_END)
	{
		free(decompressed);
		return NULL;
	}

	return decompressed;
}

uint8_t* trfs_open(const TRFILESYS* fs, const char* path, size_t* file_size)
{
	TRFSNODE* node = fs->root;
	while (*path)
	{
		char idx = transform_char(*path);
		if (node->children[idx] == NULL)
			return NULL;
		node = node->children[idx];
		++path;
	}

	if (node->file != NULL)
	{
		if (ftell(fs->bigfile) != fs->offset)
			fseek(fs->bigfile, fs->offset, SEEK_SET);

		if (node->file->pos != 0)
			fseek(fs->bigfile, node->file->pos, SEEK_CUR);

		uint8_t* compressed = (uint8_t*)malloc(sizeof(uint8_t) * node->file->size);
		if (compressed == NULL)
			return NULL;

		size_t result = fread_s(compressed, node->file->size, node->file->size, 1, fs->bigfile);
		if (result != 1)
		{
			free(compressed);
			return NULL;
		}

		*file_size = node->file->size;
		return decompress(compressed, node->file->deflated, node->file->size);
	}

	return NULL;
}


#endif //__FSLIB_H__