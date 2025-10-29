#include <AK/TypedTransfer.h>
#include <Kernel/Tasks/Process.h>
#include <Kernel/Version.h>

namespace Kernel {

#if ARCH(X86_64)
#    define UNAME_MACHINE "x86_64"
#elif ARCH(AARCH64)
#    define UNAME_MACHINE "AArch64"
#elif ARCH(RISCV64)
#    define UNAME_MACHINE "riscv64"
#else
#    error Unknown architecture
#endif

KString* g_version_string { nullptr };

ErrorOr<FlatPtr> Process::sys$uname(Userspace<utsname*> user_buf)
{
    VERIFY_NO_PROCESS_BIG_LOCK(this);
    TRY(require_promise(Pledge::stdio));

    utsname buf {
        "lxsystem",
        "lxhost",
        "0.1",
        "0.1",
        UNAME_MACHINE
    };

    TRY(copy_to_user(user_buf, &buf));
    return 0;
}

}
