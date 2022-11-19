#include <SFML/Graphics.hpp>
#include <SFML/Window.hpp>
#include <iostream>
#include <filesystem>
#include <boost/process.hpp>
#include <vector>
#include <thread>
#include <chrono>
#include <set>
#include <fstream>

class FileHandler
{
public:
    FileHandler(const std::string& path)
        : m_path(path),
          m_dir_it(m_path),
          m_likes_path("liked_photos.txt"),
          m_dislikes_path("disliked_photos.txt")
    {
        if (!readFileToSet(m_likes_path, m_liked_files))
            std::cerr << "failed to open file " << m_likes_path << std::endl;
        if (!readFileToSet(m_dislikes_path, m_disliked_files))
            std::cerr << "failed to open file " << m_dislikes_path << std::endl;
    }

    std::string getFirstFile()
    {
        std::string file = getCurrentFile();
        if (m_liked_files.contains(file) || m_disliked_files.contains(file))
            return getNextFile();
        else
            return file;
    }

    std::string getNextFile()
    {
        std::string file;
        do
        {
            ++m_dir_it;
            file = getCurrentFile();
            if (file.empty()) return file;
        }
        while (m_liked_files.contains(file) || m_disliked_files.contains(file));
        std::cout << "looked at " << m_liked_files.size() + m_disliked_files.size() << " photos" << std::endl;
        return file;
    }

    std::string getCurrentFile() const
    {
        if (m_dir_it == std::filesystem::directory_iterator())
            return "";

        const std::filesystem::directory_entry& file = *m_dir_it;
        const std::filesystem::path& path = file.path();
        return path.string();
    }

    void likeCurrentFile()
    {
        m_liked_files.insert(getCurrentFile());
    }

    void dislikeCurrentFile()
    {
        m_disliked_files.insert(getCurrentFile());
    }

    void writeLikesAndDislikes()
    {
        if (!writeSetToFile(m_liked_files, m_likes_path))
            std::cerr << "failed to write file " << m_likes_path << std::endl;
        if (!writeSetToFile(m_disliked_files, m_dislikes_path))
            std::cerr << "failed to write file " << m_dislikes_path << std::endl;
    }

    ~FileHandler()
    {
        writeLikesAndDislikes();
    }

    FileHandler() = delete;
    FileHandler(const FileHandler&) = delete;
    FileHandler(FileHandler&&) = delete;
    FileHandler& operator=(const FileHandler&) = delete;
    FileHandler& operator=(FileHandler&&) = delete;
    
private:
    std::filesystem::path m_path;
    std::filesystem::directory_iterator m_dir_it;

    const std::string m_likes_path;
    const std::string m_dislikes_path;

    std::set<std::string> m_liked_files;
    std::set<std::string> m_disliked_files;

    bool readFileToSet(const std::string& input_path, std::set<std::string>& output)
    {
        std::ifstream file(input_path);
        if (!file.is_open())
            return false;
        std::string line;
        while (std::getline(file, line))
            output.insert(line);
        return true;
    }

    bool writeSetToFile(const std::set<std::string>& input, const std::string& output_path)
    {
        std::ofstream file(output_path);
        if (!file.is_open())
            return false;
        for (const std::string& str: input)
            file << str << std::endl;
        return true;
    }

};

class PhotoViewer : public sf::RenderWindow
{
public:
    PhotoViewer(unsigned int max_w, unsigned int max_h, const std::string& title, const std::string& path)
        : sf::RenderWindow(sf::VideoMode(max_w, max_h), title),
          m_max_w(max_w),
          m_max_h(max_h),
          m_file_handler(path)
    {
        this->setFramerateLimit(30);

        loadImage(m_file_handler.getFirstFile());
    }

    bool loadImage(const std::string& filename)
    {
        if (!m_image.loadFromFile(filename))
            return false;
        if (!m_texture.loadFromImage(m_image))
            return false;        
        m_image_filename = filename;
        m_texture.setSmooth(true);
        m_sprite.setTexture(m_texture, true);

        // figure out how to scale the image while maintaining aspect ratio
        // and respecting max window size.
        sf::Vector2u image_size = m_image.getSize();

        float aspect_ratio = static_cast<float>(image_size.x) / static_cast<float>(image_size.y);
        sf::Vector2u max_window_size(m_max_w, m_max_h);
        float scale = 1.0f;
        if ((image_size.x > max_window_size.x) || (image_size.y > max_window_size.y))
        {
            // the image is bigger than the window, so it must be scaled

            //  aspect_ratio = w / h    -->    w = h * aspect_ratio
            //                          -->    h = w / aspect_ratio

            // try max image height
            float potential_w = m_max_h * aspect_ratio;
            if (potential_w < m_max_w)
            {
                // new_h = scale * old_h --> scale = new_h / old_h
                // where: new_h = max_h
                // no need to calculate scale in the other dimension since we maintain aspect ratio.
                scale = static_cast<float>(m_max_h) / static_cast<float>(image_size.y);
                m_sprite.setScale(scale, scale);

                sf::Vector2u window_size(static_cast<unsigned int>(potential_w),
                                         static_cast<unsigned int>(scale * static_cast<float>(image_size.y)));
                this->setSize(window_size);
            }
            else
            {
                // max image height is too wide, so use max image width
                float potential_h = m_max_w / aspect_ratio;
                
                // new_w = scale * old_w --> scale = new_w / old_w
                // where: new_w = max_w
                // no need to calculate scale in other dimension since we maintain aspect ratio.
                scale = static_cast<float>(m_max_w) / static_cast<float>(image_size.x);                
                m_sprite.setScale(scale, scale);

                sf::Vector2u window_size(static_cast<unsigned int>(scale * static_cast<float>(image_size.x)),
                                         static_cast<unsigned int>(potential_h));
                this->setSize(window_size);
            }
        }
        else
        {
            // the image is smaller than the maximum window size, so just set the window
            // to the image size.
            this->setSize(image_size);
        }

        // correct window view
        sf::Vector2u window_size = this->getSize();
        sf::View view(sf::FloatRect(0.0f, 0.0f, window_size.x, window_size.y));
        this->setView(view);
        // center the window
        unsigned int center_x = sf::VideoMode::getDesktopMode().width / 2;
        unsigned int center_y = sf::VideoMode::getDesktopMode().height / 2;
        unsigned int corner_x = center_x - (window_size.x / 2);
        unsigned int corner_y = center_y - (window_size.y / 2);
        this->setPosition(sf::Vector2i(corner_x, corner_y));

        return true;
    }

    void show()
    {
        while (this->isOpen())
        {
            sf::Event event;
            while (this->pollEvent(event))
            {
                switch (event.type)
                {
                    case sf::Event::Closed:
                    {
                        this->close();
                    }
                    break;

                    case sf::Event::KeyReleased:
                    {
                        if (event.key.code == sf::Keyboard::Escape)
                        {
                            this->close();
                        }
                        bool load_next_image = false;
                        if (event.key.code == sf::Keyboard::Enter)
                        {
                            m_file_handler.likeCurrentFile();
                            load_next_image = true;
                        }
                        if (event.key.code == sf::Keyboard::Backspace)
                        {
                            m_file_handler.dislikeCurrentFile();
                            load_next_image = true;
                        }

                        if (load_next_image)
                        {
                            std::string next_image = m_file_handler.getNextFile();
                            if (next_image.empty())
                            {
                                this->close();
                            }
                            else
                            {
                                loadImage(next_image);
                            }
                        }
                    }
                    break;
                }
            }

            this->clear(sf::Color::Black);
            this->draw(m_sprite);
            this->display();
        }
    }

    ~PhotoViewer() = default;

    PhotoViewer() = delete;
    PhotoViewer(const PhotoViewer&) = delete;
    PhotoViewer(PhotoViewer&&) = delete;
    PhotoViewer& operator=(const PhotoViewer&) = delete;
    PhotoViewer& operator=(PhotoViewer&&) = delete;
    
private:
    sf::RenderWindow m_window;
    unsigned int m_max_w;
    unsigned int m_max_h;
    std::string m_image_filename;
    sf::Image m_image;
    sf::Texture m_texture;
    sf::Sprite m_sprite;

    FileHandler m_file_handler;
};

void print_usage(const std::string& prog_name)
{
    std::cerr << "Usage:" << std::endl;
    std::cerr << "\t" << prog_name << " photos_dir" << std::endl;
}

int main(int argc, char* argv[])
{
    if (argc != 2)
    {
        print_usage(argv[0]);
        return 1;
    }

    std::string photo_path(argv[1]);

    unsigned int padding_w = 100;
    unsigned int padding_h = 100;
    unsigned int screen_w = sf::VideoMode::getDesktopMode().width;
    unsigned int screen_h = sf::VideoMode::getDesktopMode().height;

    unsigned int max_w = screen_w - padding_w;
    unsigned int max_h = screen_h - padding_h;

    // try to avoid an event from the enter key release from running via CLI
    std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(250));
    
    PhotoViewer v(max_w, max_h, "P H O T O S", photo_path);
    v.show();

    return 0;
}
