#include "missing_classes.h";
class MenuScreen : public Screen
{
public:
    MenuScreen();
    void update(InputState &input);
    void render();
    void sendMessage(const char *message, const char *value);
    void dialogFinished(const Screen *dialog, DialogResult result);
private:
    int frames_;
};

class SettingsScreen : public Screen
{
public:
    void update(InputState &input);
    void render();
};

class AudioScreen : public Screen
{
public:
    void update(InputState &input);
    void render();
};

class GraphicsScreen : public Screen
{
public:
    void update(InputState &input);
void render();
};

class SystemScreen : public Screen
{
public: void update(InputState &input);
void render();
};

class FileSelectScreen : public Screen
{
public: FileSelectScreen(const FileSelectScreenOptions &options);
    void update(InputState &input);
    void render();
    virtual void onSelectFile() {}
    virtual void onCancel() {}
    void key(const KeyInput &key);

private:
    void updateListing();

    FileSelectScreenOptions options_;
    UIList list_;
    //std::string currentDirectory_;
    //std::vector<FileInfo> listing_;
};
