class MenuScreen : public Screen
{
public:
    MenuScreen()
    void update(InputState &input);
    void render();
    void sendMessage(const char *message, const char *value);
    void dialogFinished(const Screen *dialog, DialogResult result);
private:
    int frames_;
};
