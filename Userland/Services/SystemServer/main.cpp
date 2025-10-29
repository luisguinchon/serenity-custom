/*
 * Copyright (c) 2018-2021, Andreas Kling <kling@serenityos.org>
 * Copyright (c) 2021, Peter Elliott <pelliott@serenityos.org>
 * Copyright (c) 2023, Liav A. <liavalb@hotmail.co.il>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Service.h"
#include <AK/Assertions.h>
#include <AK/ByteBuffer.h>
#include <AK/Debug.h>
#include <AK/String.h>
#include <Kernel/API/DeviceEvent.h>
#include <LibCore/ArgsParser.h>
#include <LibCore/ConfigFile.h>
#include <LibCore/DirIterator.h>
#include <LibCore/Event.h>
#include <LibCore/EventLoop.h>
#include <LibCore/File.h>
#include <LibCore/System.h>
#include <LibGUI/Application.h>
#include <LibGUI/Window.h>
#include <LibGUI/Widget.h>
#include <LibGUI/Painter.h>
#include <LibGfx/Bitmap.h>
#include <LibMain/Main.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <signal.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

static constexpr StringView text_system_mode = "text"sv;
static constexpr StringView selftest_system_mode = "self-test"sv;
static constexpr StringView graphical_system_mode = "graphical"sv;
ByteString g_system_mode = graphical_system_mode;
Vector<NonnullRefPtr<Service>> g_services;

// NOTE: This handler ensures that the destructor of g_services is called.
static void sigterm_handler(int)
{
    exit(0);
}

static void sigchld_handler(int)
{
    for (;;) {
        int status = 0;
        pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid < 0) {
            perror("waitpid");
            break;
        }
        if (pid == 0)
            break;

        dbgln_if(SYSTEMSERVER_DEBUG, "Reaped child with pid {}, exit status {}", pid, status);

        Service* service = Service::find_by_pid(pid);
        if (service == nullptr) {
            continue;
        }

        if (auto result = service->did_exit(status); result.is_error())
            dbgln("{}: {}", service->name(), result.release_error());
    }
}

namespace SystemServer {

static ErrorOr<void> determine_system_mode()
{
    ArmedScopeGuard declare_text_mode_on_failure([&] {
        if (g_system_mode != selftest_system_mode)
            g_system_mode = text_system_mode;
    });

    auto file_or_error = Core::File::open("/sys/kernel/system_mode"sv, Core::File::OpenMode::Read);
    if (file_or_error.is_error()) {
        dbgln("Failed to read system_mode: {}", file_or_error.error());
        return {};
    }
    auto const system_mode_buf_or_error = file_or_error.value()->read_until_eof();
    if (system_mode_buf_or_error.is_error()) {
        dbgln("Failed to read system_mode: {}", system_mode_buf_or_error.error());
        return {};
    }
    ByteString const system_mode = ByteString::copy(system_mode_buf_or_error.value(), Chomp);

    g_system_mode = system_mode;
    declare_text_mode_on_failure.disarm();

    dbgln("Read system_mode: {}", g_system_mode);
    return {};
}

static ErrorOr<void> activate_services(Core::ConfigFile const& config)
{
    Vector<NonnullRefPtr<Service>> services_to_activate;
    for (auto const& name : config.groups()) {
        auto service = TRY(Service::try_create(config, name));
        if (service->is_enabled_for_system_mode(g_system_mode)) {
            TRY(service->setup_sockets());
            g_services.append(move(service));
            services_to_activate.append(g_services.last());
        }
    }

    dbgln("Activating {} services...", services_to_activate.size());
    for (auto& service : services_to_activate) {
        dbgln_if(SYSTEMSERVER_DEBUG, "Activating {}", service->name());
        if (auto result = service->activate(); result.is_error())
            dbgln("{}: {}", service->name(), result.release_error());
    }
    return {};
}

static ErrorOr<void> activate_base_services_based_on_system_mode()
{
    if (g_system_mode == graphical_system_mode) {
        bool found_gpu_device = false;
        for (int attempt = 0; attempt < 10; attempt++) {
            struct stat file_state;
            int rc = lstat("/dev/gpu/connector0", &file_state);
            if (rc == 0) {
                found_gpu_device = true;
                break;
            }
            sleep(1);
        }
        if (!found_gpu_device) {
            dbgln("WARNING: No device nodes at /dev/gpu/ directory after 10 seconds. Disabling graphics mode.");
            g_system_mode = text_system_mode;
        }
    }

    auto config = TRY(Core::ConfigFile::open_for_system("SystemServer"));
    TRY(activate_services(*config));
    return {};
}

static ErrorOr<void> activate_user_services_based_on_system_mode()
{
    auto config = TRY(Core::ConfigFile::open_for_system("SystemServerUser"));
    TRY(activate_services(*config));

    if (auto config = Core::ConfigFile::open_for_app("SystemServer"); !config.is_error()) {
        TRY(activate_services(config.release_value()));
    }
    return {};
}

};

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    bool user = false;
    Core::ArgsParser args_parser;
    args_parser.add_option(user, "Run in user-mode", "user", 'u');
    args_parser.parse(arguments);

    TRY(Core::System::pledge("stdio proc exec tty accept unix rpath wpath cpath chown fattr id sigaction"));

    if (!user)
        TRY(SystemServer::determine_system_mode());

    Core::EventLoop event_loop;
    event_loop.register_signal(SIGCHLD, sigchld_handler);
    event_loop.register_signal(SIGTERM, sigterm_handler);

    // ----- LXsystem Boot Logo -----
    if (!user && g_system_mode == graphical_system_mode) {
        dbgln("Displaying LXsystem boot logo...");

        auto app = MUST(GUI::Application::try_create(arguments));
        auto logo_or_error = Gfx::Bitmap::load_from_file("/res/logo/logo.png"sv);

        if (!logo_or_error.is_error()) {
            auto logo = logo_or_error.release_value();
            auto splash = MUST(GUI::Window::try_create());
            splash->set_title("LXsystem Boot");
            splash->resize(logo->width(), logo->height());
            splash->set_resizable(false);
            splash->set_double_buffering_enabled(true);
            splash->set_frameless(true);
            splash->center_on_screen();

            auto widget = splash->set_main_widget<GUI::Widget>();
            widget->set_fill_with_background_color(true);
            widget->on_paint = [logo](GUI::PaintEvent&) {
                GUI::Painter painter(*GUI::Widget::the());
                painter.clear_rect({ 0, 0, logo->width(), logo->height() }, Gfx::Color::Black);
                painter.draw_bitmap({ 0, 0 }, *logo);
            };

            splash->show();
            Core::EventLoop::current().pump();
            usleep(2500000); // 2.5s splash screen
        }
    }
    // ------------------------------

    if (!user)
        TRY(SystemServer::activate_base_services_based_on_system_mode());
    else
        TRY(SystemServer::activate_user_services_based_on_system_mode());

    return event_loop.exec();
}
