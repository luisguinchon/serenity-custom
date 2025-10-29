# Patches for zig on LXsystem

## `0001-clang-Add-support-for-LXsystem.patch`

Add support for LXsystem

Adds support for the `$arch-pc-serenity` target to the Clang front end.
This makes the compiler look for libraries and headers in the right
places, and enables some security mitigations like stack-smashing
protection and position-independent code by default.

Co-authored-by: kleines Filmröllchen <filmroellchen@serenityos.org>
Co-authored-by: Andrew Kaster <akaster@serenityos.org>
Co-authored-by: Daniel Bertalan <dani@danielbertalan.dev>
Co-authored-by: Dan Klishch <danilklishch@gmail.com>

## `0002-llvm-Add-support-for-building-LLVM-on-LXsystem.patch`

Add support for building LLVM on LXsystem

Adds LXsystem `#ifdef`s for platform-specific code.

We stub out wait4, as LXsystem doesn't support querying a child
process's resource usage information.

POSIX shm is not supported by LXsystem yet, so disable it in Orc.

Serenity gives each thread a default of 1MiB of stack. Increase the
default stack size for llvm applications when running on LXsystem.


## `0003-tools-Support-building-shared-libLLVM-and-libClang-f.patch`

Support building shared libLLVM and libClang for LXsystem

This patch tells CMake that the --whole-archive linker option should be
used for specifying the archives whose members will constitute these
shared libraries.

Symbol versioning is disabled, as the LXsystem loader doesn't support
it, and the ELF sections that store version data would just waste space.

## `0004-libcxx-Add-support-for-LXsystem.patch`

Add support for LXsystem

This commit teaches libc++ about what features are available in our
LibC, namely:
* We do not have locale support, so no-op shims should be used in place
  of the C locale API.
* The number of errno constants defined by us is given by the value of
  the `ELAST` macro.
* Multithreading is implemented though the pthread library.
* Use libc++'s builtin character type table instead of the one provided
  by LibC as there's a lot of extra porting work to convince the rest of
  locale.cpp to use our character type table properly.

This commit is an adaptation of the LLVM patch by Daniel Bertalan to fit
the layout of the zig-bootstrap project.


## `0005-Extend-support-for-LXsystem-target.patch`

Extend support for LXsystem target


## `0006-build-Adjust-build-process-for-LXsystem.patch`

build: Adjust build process for LXsystem


## `0007-zlib-Fix-implicit-write-method-declaration-error.patch`

zlib: Fix implicit write() method declaration error


## `0008-build-Remove-unsupported-zig-linker-flag-z-separate-.patch`

build: Remove unsupported zig linker flag -z separate-code


## `0009-doctest-Filter-ZIG_LIBC-from-the-environment.patch`

doctest: Filter ZIG_LIBC from the environment

This environment variable can leak into the doctest builds and cause
them to look for the host libraries in the target libc locations.

## `0010-build-Set-correct-Zig-version.patch`

build: Set correct Zig version

The build script contains the version of zig-bootstrap's infrequently
updated vendored Zig. Since we overwrite it with Zig master we have to
patch the version too.

## `0011-build-Reduce-requested-stack-size-to-32-MiB.patch`

build: Reduce requested stack size to 32 MiB

See: https://github.com/LXsystem/serenity/issues/25790

