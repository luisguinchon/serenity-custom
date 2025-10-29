#include <LibGUI/Application.h>
#include <LibGUI/Window.h>
#include <LibGUI/Painter.h>
#include <LibGUI/Widget.h>
#include <LibGUI/Timer.h>
#include <LibGUI/Event.h>
#include <LibMain/Main.h>
#include <AK/Random.h>

class PongWidget final : public GUI::Widget {
    C_OBJECT(PongWidget);

public:
    PongWidget()
    {
        set_fill_with_background_color(true);
        set_background_role(Gfx::ColorRole::Window);
        start_timer(16); // ~60 FPS
        reset_game();
    }

private:
    Gfx::IntRect m_ball;
    Gfx::IntRect m_paddle_left;
    Gfx::IntRect m_paddle_right;
    Gfx::IntPoint m_ball_velocity;
    int m_speed = 6;
    int m_score_left = 0;
    int m_score_right = 0;

    void reset_game()
    {
        auto size = this->size();
        m_ball = { size.width() / 2 - 5, size.height() / 2 - 5, 10, 10 };
        m_paddle_left = { 20, size.height() / 2 - 30, 8, 60 };
        m_paddle_right = { size.width() - 28, size.height() / 2 - 30, 8, 60 };
        m_ball_velocity = { (get_random<int>() % 2 == 0 ? m_speed : -m_speed),
                            (get_random<int>() % 2 == 0 ? m_speed : -m_speed) };
    }

    virtual void timer_event(Core::TimerEvent&) override
    {
        m_ball.translate_by(m_ball_velocity);

        // collision top/bottom
        if (m_ball.top() <= 0 || m_ball.bottom() >= height())
            m_ball_velocity.set_y(-m_ball_velocity.y());

        // paddle collision
        if (m_ball.intersects(m_paddle_left) || m_ball.intersects(m_paddle_right)) {
            m_ball_velocity.set_x(-m_ball_velocity.x());
        }

        // scoring
        if (m_ball.left() <= 0) {
            m_score_right++;
            reset_game();
        } else if (m_ball.right() >= width()) {
            m_score_left++;
            reset_game();
        }

        update();
    }

    virtual void keydown_event(GUI::KeyEvent& event) override
    {
        int move = 15;
        if (event.key() == Key_W && m_paddle_left.top() > 0)
            m_paddle_left.translate_by(0, -move);
        if (event.key() == Key_S && m_paddle_left.bottom() < height())
            m_paddle_left.translate_by(0, move);
        if (event.key() == Key_Up && m_paddle_right.top() > 0)
            m_paddle_right.translate_by(0, -move);
        if (event.key() == Key_Down && m_paddle_right.bottom() < height())
            m_paddle_right.translate_by(0, move);
    }

    virtual void paint_event(GUI::PaintEvent& event) override
    {
        GUI::Painter painter(*this);
        painter.fill_rect(event.rect(), Color::Black);
        painter.fill_rect(m_paddle_left, Color::Red);
        painter.fill_rect(m_paddle_right, Color::Red);
        painter.fill_rect(m_ball, Color::White);

        painter.draw_text(Gfx::IntRect(0, 0, width(), 20),
            String::formatted("{} : {}", m_score_left, m_score_right),
            Gfx::TextAlignment::Center, Color::White);
    }
};

ErrorOr<int> serenity_main(Main::Arguments arguments)
{
    auto app = TRY(GUI::Application::create(arguments));
    auto window = TRY(GUI::Window::create());
    window->resize(640, 400);
    window->set_title("Pong");
    window->set_main_widget<PongWidget>();
    window->show();
    return app->exec();
}
