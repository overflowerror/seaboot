# seaboot - Bootstrap C-Projects

## What is this?
Another library to simplify developement of C programs.

## How do I use this?

Make sure you got autotools installed.

First update the generated configuration files

```bash
$ autoreconf --install
```

Then run the `./configure` script:

```bash
$ ./configure
```

The last step is to build both the library and the example program.
```bash
$ make
```

The compiled files are in `./bin/`.

## TODO

- simplified socket-management
- simplified process-management (fork/exec, shm, semaphores, pthread, ...)
