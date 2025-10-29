// Apps/StorageManager/main.cpp
#include <AK/String.h>
#include <AK/Vector.h>
#include <LibCore/Command.h>
#include <LibCore/DirIterator.h>
#include <LibCore/System.h>
#include <LibGUI/Application.h>
#include <LibGUI/BoxLayout.h>
#include <LibGUI/Button.h>
#include <LibGUI/ComboBox.h>
#include <LibGUI/MessageBox.h>
#include <LibGUI/TableView.h>
#include <LibGUI/TextBox.h>
#include <LibGUI/Window.h>
#include <LibGUI/Model.h>
#include <LibGUI/Progressbar.h>

struct BlockDevice {
    String path;     // ex: /dev/hda, /dev/sdb1
    String size;     // ex: "14.7G"
    String mountpoint;// ex: "/mnt/usb" ou vide
};

class DevicesModel final : public GUI::Model {
    C_OBJECT(DevicesModel)
public:
    enum Column { Path, Size, Mountpoint, __Count };
    virtual int row_count(const GUI::ModelIndex&) const override { return m_devices.size(); }
    virtual int column_count(const GUI::ModelIndex&) const override { return __Count; }
    virtual String column_name(int column) const override {
        switch (column) {
        case Path: return "Device";
        case Size: return "Taille";
        case Mountpoint: return "Monté sur";
        default: return {};
        }
    }
    virtual GUI::Variant data(const GUI::ModelIndex& index, GUI::ModelRole role) const override {
        if (role != GUI::ModelRole::Display)
            return {};
        auto const& d = m_devices.at(index.row());
        if (index.column() == Path) return d.path;
        if (index.column() == Size) return d.size;
        if (index.column() == Mountpoint) return d.mountpoint;
        return {};
    }

    void refresh()
    {
        m_devices.clear();

        // 1) Parcours /dev pour les block devices simples
        Core::DirIterator it("/dev", Core::DirIterator::SkipDots);
        while (it.has_next()) {
            auto name = it.next_path();
            if (!name.starts_with("sd") && !name.starts_with("hd") && !name.starts_with("vd"))
                continue;
            String path = String::formatted("/dev/{}", name);

            // Taille via `stat -c %s` fallback: unknown
            String size = "inconnue";
            auto stat_out = Core::Command::run_command("stat", { "-c", "%s", path });
            if (stat_out.is_success())
                size = stat_out.stdout_string().trim_whitespace();

            // Mountpoint via `mount` grep
            String mountpoint;
            auto m = Core::Command::run_command("mount", {});
            if (m.is_success()) {
                for (auto& line : m.stdout_string().split_view('\n')) {
                    if (line.contains(path))
                        mountpoint = String(line).trim_whitespace();
                }
            }
            m_devices.append({ path, size, mountpoint });
        }
        did_update();
    }

private:
    Vector<BlockDevice> m_devices;
};

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    auto app = TRY(GUI::Application::try_create(arguments));
    auto window = TRY(GUI::Window::try_create());
    window->set_title("Storage Manager");
    window->resize(680, 420);

    auto& root = window->set_main_widget<GUI::Widget>();
    root.set_layout<GUI::VerticalBoxLayout>();

    auto& table = root.add<GUI::TableView>();
    auto model = adopt_ref(*new DevicesModel());
    table.set_model(model);

    auto& actions = root.add<GUI::Widget>();
    actions.set_fixed_height(44);
    actions.set_layout<GUI::HorizontalBoxLayout>();

    auto& mount_point_box = actions.add<GUI::TextBox>();
    mount_point_box.set_placeholder("Point de montage, ex: /mnt/usb");

    auto& fs_combo = actions.add<GUI::ComboBox>();
    fs_combo.set_model(*GUI::StringListModel::create({ "ext2", "fat" })); // ajuste selon ce qui existe dans ta build
    fs_combo.set_selected_index(0);

    auto& btn_refresh = actions.add<GUI::Button>("Rafraîchir");
    auto& btn_mount   = actions.add<GUI::Button>("Monter");
    auto& btn_umount  = actions.add<GUI::Button>("Démonter");
    auto& btn_format  = actions.add<GUI::Button>("Formater");

    btn_refresh.on_click = [&](auto) { model->refresh(); };

    btn_mount.on_click = [&](auto) {
        auto index = table.selection().first();
        if (!index.is_valid()) {
            GUI::MessageBox::show(window, "Sélectionne un device.", "Erreur", GUI::MessageBox::Type::Error);
            return;
        }
        auto dev = model->index(index.row(), DevicesModel::Path).data().to_string();
        auto mnt = mount_point_box.text();
        if (mnt.is_empty()) {
            GUI::MessageBox::show(window, "Donne un point de montage.", "Erreur", GUI::MessageBox::Type::Error);
            return;
        }
        auto res = Core::Command::run_command("mkdir", { "-p", mnt });
        if (!res.is_success()) {
            GUI::MessageBox::show(window, "Impossible de créer le dossier.", "Erreur", GUI::MessageBox::Type::Error);
            return;
        }
        auto r = Core::Command::run_command("mount", { dev, mnt });
        if (!r.is_success())
            GUI::MessageBox::show(window, String::formatted("Échec mount: {}", r.stderr_string()), "Erreur", GUI::MessageBox::Type::Error);
        model->refresh();
    };

    btn_umount.on_click = [&](auto) {
        auto index = table.selection().first();
        if (!index.is_valid()) return;
        auto dev = model->index(index.row(), DevicesModel::Path).data().to_string();
        auto r = Core::Command::run_command("umount", { dev });
        if (!r.is_success())
            GUI::MessageBox::show(window, String::formatted("Échec umount: {}", r.stderr_string()), "Erreur", GUI::MessageBox::Type::Error);
        model->refresh();
    };

    btn_format.on_click = [&](auto) {
        auto index = table.selection().first();
        if (!index.is_valid()) return;
        auto dev = model->index(index.row(), DevicesModel::Path).data().to_string();
        auto fs  = fs_combo.text();

        // ATTENTION: ça efface tout. On exige umount avant.
        auto u = Core::Command::run_command("umount", { dev });
        (void)u;

        Vector<String> cmd;
        if (fs == "ext2")
            cmd = { "mkfs-ext2", dev };
        else if (fs == "fat")
            cmd = { "mkfs-fat", dev };
        else {
            GUI::MessageBox::show(window, "FS non supporté.", "Erreur", GUI::MessageBox::Type::Error);
            return;
        }
        auto r = Core::Command::run_command(cmd.at(0), { cmd.at(1) });
        if (!r.is_success())
            GUI::MessageBox::show(window, String::formatted("Échec format: {}", r.stderr_string()), "Erreur", GUI::MessageBox::Type::Error);
        model->refresh();
    };

    model->refresh();
    window->show();
    return app->exec();
}
