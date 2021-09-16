# Launcher partition

The purpose of this project is to illustrate, using a simple example, how to
transfer the execution flow from the root partition to a child partition.

## Project structure

The project structure is the following:

```
.
├── doc
├── Doxyfile
├── include
│   └── launcher.h
├── link.ld
├── main.c
├── Makefile
├── minimal
│   ├── link.ld
│   ├── main.c
│   └── Makefile
├── partition.S
└── README.md
```

The root partition code can be found at the root of the project in the `main.c` file.

The child partition code can be found in the `main.c` file in the `minimal` directory.

## Documentation

You can generate the project documentation with the following command:

```console
$ make doc
```

You'll find it in the `doc` directory.
