# FSLib
A simple header-only virtual filesystem library

For testing I used the [winlibs/liblzma](https://github.com/winlibs/liblzma) distribution.
However any distribution of liblzma compatible with your platform should work.

This implementation is purely academic, as it uses a trie to store file data
instead of a hashmap table.

It's meant to be used as part of 2 programs, a packer and your program/game/etc.

The packer must define `PACK_BIGFILE`, include `FSLib.h` and perform these operations:
```TRFILESYS* fs = create_trfs();
insert_trfs(fs, "D:\\clock.png", "images\\clock.png");
insert_trfs(fs, "D:\\arrow.png", "images\\arrow.png");
insert_trfs(fs, "D:\\music.ogg", "audio\\music.ogg");
// etc.
write_trfs(fs, "D:\\data.big");
unload_trfs(&fs);
```
In your program/game/app, **don't define `PACK_BIGFILE`**, only include `FSLib.h`.
To read files perform these operations:
```TRFILESYS* fs_ld = load_trfs("D:\\data.big");
size_t file_size;
uint8_t* filebuf = trfs_open(fs_ld, "audio\\oggmb.ogg", &file_size);
// do something with filebuf
free(filebuf);
```