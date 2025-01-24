#include <iostream>
#include <emscripten.h>
#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>
#include <algorithm>

struct File {
    std::string name;
    std::string content;
    bool is_directory;
    std::unordered_map<std::string, File*> children;

    File(std::string name, bool is_directory)
        : name(name), is_directory(is_directory) {}
};

File* root = new File("/", true);
File* current_directory = root;

std::vector<std::string> split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

File* navigate_to(const std::string& path) {
    File* dir = (path[0] == '/') ? root : current_directory;
    std::vector<std::string> parts = split(path, '/');
    for (const auto& part : parts) {
        if (part == "..") {
            if (dir->name != "/") dir = dir->children[".."];
        } else if (!part.empty() && part != ".") {
            if (dir->children.find(part) == dir->children.end()) return nullptr;
            dir = dir->children[part];
        }
    }
    return dir;
}

void ls(const std::string& args) {
    File* dir = navigate_to(args);
    if (dir && dir->is_directory) {
        for (const auto& child : dir->children) {
            if (child.first != "..") {
                EM_ASM({
                    var outputElement = document.getElementById('output');
                    outputElement.innerHTML += UTF8ToString($0) + '<br>';
                }, child.first.c_str());
            }
        }
    } else {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Directory not found<br>';
        });
    }
}

void pwd(const std::string& args) {
    std::string path;
    for (File* dir = current_directory; dir != root; dir = dir->children[".."]) {
        path = "/" + dir->name + path;
    }
    path = (path.empty()) ? "/" : path;
    EM_ASM({
        var outputElement = document.getElementById('output');
        outputElement.innerHTML += UTF8ToString($0) + '<br>';
    }, path.c_str());
}

void mkdir(const std::string& args) {
    File* dir = navigate_to(args.substr(0, args.find_last_of('/')));
    if (dir && dir->is_directory) {
        std::string name = args.substr(args.find_last_of('/') + 1);
        dir->children[name] = new File(name, true);
        dir->children[name]->children[".."] = dir; // Parent directory
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Directory created<br>';
        });
    } else {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Failed to create directory<br>';
        });
    }
}

void rmdir(const std::string& args) {
    File* dir = navigate_to(args);
    if (dir && dir->is_directory && dir->children.size() == 1) { // Only contains ".."
        dir->children[".."]->children.erase(dir->name);
        delete dir;
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Directory removed<br>';
        });
    } else {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Failed to remove directory<br>';
        });
    }
}

void touch(const std::string& args) {
    File* dir = navigate_to(args.substr(0, args.find_last_of('/')));
    if (dir && dir->is_directory) {
        std::string name = args.substr(args.find_last_of('/') + 1);
        dir->children[name] = new File(name, false);
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'File created<br>';
        });
    } else {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Failed to create file<br>';
        });
    }
}

void rm(const std::string& args) {
    File* dir = navigate_to(args.substr(0, args.find_last_of('/')));
    if (dir && dir->is_directory) {
        std::string name = args.substr(args.find_last_of('/') + 1);
        if (dir->children.find(name) != dir->children.end() && !dir->children[name]->is_directory) {
            delete dir->children[name];
            dir->children.erase(name);
            EM_ASM({
                var outputElement = document.getElementById('output');
                outputElement.innerHTML += 'File removed<br>';
            });
        } else {
            EM_ASM({
                var outputElement = document.getElementById('output');
                outputElement.innerHTML += 'Failed to remove file<br>';
            });
        }
    } else {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Failed to remove file<br>';
        });
    }
}

void cat(const std::string& args) {
    File* file = navigate_to(args);
    if (file && !file->is_directory) {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += UTF8ToString($0) + '<br>';
        }, file->content.c_str());
    } else {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Failed to read file<br>';
        });
    }
}

void cp(const std::string& args) {
    std::vector<std::string> files = split(args, ' ');
    if (files.size() != 2) {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Invalid arguments<br>';
        });
        return;
    }
    File* src_file = navigate_to(files[0]);
    File* dest_dir = navigate_to(files[1].substr(0, files[1].find_last_of('/')));
    if (src_file && !src_file->is_directory && dest_dir && dest_dir->is_directory) {
        std::string name = files[1].substr(files[1].find_last_of('/') + 1);
        dest_dir->children[name] = new File(name, false);
        dest_dir->children[name]->content = src_file->content;
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'File copied<br>';
        });
    } else {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Failed to copy file<br>';
        });
    }
}

void mv(const std::string& args) {
    std::vector<std::string> files = split(args, ' ');
    if (files.size() != 2) {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Invalid arguments<br>';
        });
        return;
    }
    File* src_file = navigate_to(files[0]);
    File* dest_dir = navigate_to(files[1].substr(0, files[1].find_last_of('/')));
    if (src_file && !src_file->is_directory && dest_dir && dest_dir->is_directory) {
        std::string name = files[1].substr(files[1].find_last_of('/') + 1);
        dest_dir->children[name] = src_file;
        src_file->children[".."]->children.erase(src_file->name);
        src_file->name = name;
        src_file->children[".."] = dest_dir;
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'File moved<br>';
        });
    } else {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Failed to move file<br>';
        });
    }
}

void tar(const std::string& args) {
    std::vector<std::string> parts = split(args, ' ');
    if (parts.size() < 2 || parts[0] != "-xvf") {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Invalid tar command<br>';
        });
        return;
    }
    std::string filename = parts[1];
    File* file = navigate_to(filename);
    if (file && !file->is_directory) {
        // Simulate extracting files from tar (not a real tar extraction)
        File* extract_dir = current_directory;
        std::string content = file->content;
        std::vector<std::string> files = split(content, '\n');
        for (const auto& f : files) {
            if (!f.empty()) {
                std::vector<std::string> file_parts = split(f, ':');
                if (file_parts.size() == 2) {
                    std::string name = file_parts[0];
                    std::string file_content = file_parts[1];
                    extract_dir->children[name] = new File(name, false);
                    extract_dir->children[name]->content = file_content;
                }
            }
        }
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Files extracted<br>';
        });
    } else {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'File not found<br>';
        });
    }
}

void unzip(const std::string& args) {
    std::vector<std::string> parts = split(args, ' ');
    if (parts.size() != 1) {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Invalid unzip command<br>';
        });
        return;
    }
    std::string filename = parts[0];
    File* file = navigate_to(filename);
    if (file && !file->is_directory) {
        // Simulate extracting files from zip (not a real zip extraction)
        File* extract_dir = current_directory;
        std::string content = file->content;
        std::vector<std::string> files = split(content, '\n');
        for (const auto& f : files) {
            if (!f.empty()) {
                std::vector<std::string> file_parts = split(f, ':');
                if (file_parts.size() == 2) {
                    std::string name = file_parts[0];
                    std::string file_content = file_parts[1];
                    extract_dir->children[name] = new File(name, false);
                    extract_dir->children[name]->content = file_content;
                }
            }
        }
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Files extracted<br>';
        });
    } else {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'File not found<br>';
        });
    }
}

void execute_command(const std::string &command) {
    std::string cmd;
    std::string args;
    size_t space_pos = command.find(' ');
    if (space_pos != std::string::npos) {
        cmd = command.substr(0, space_pos);
        args = command.substr(space_pos + 1);
    } else {
        cmd = command;
    }

    if (cmd == "exit") {
        EM_ASM({
            alert('Exiting the terminal.');
        });
        emscripten_cancel_main_loop();
    } else if (cmd == "ls") {
        ls(args);
    } else if (cmd == "pwd") {
        pwd(args);
    } else if (cmd == "mkdir") {
        mkdir(args);
    } else if (cmd == "rmdir") {
        rmdir(args);
    } else if (cmd == "touch") {
        touch(args);
    } else if (cmd == "rm") {
        rm(args);
    } else if (cmd == "cat") {
        cat(args);
    } else if (cmd == "cp") {
        cp(args);
    } else if (cmd == "mv") {
        mv(args);
    } else if (cmd == "tar") {
        tar(args);
    } else if (cmd == "unzip") {
        unzip(args);
    } else if (cmd == "clear") {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML = '';
        });
    } else {
        EM_ASM({
            var outputElement = document.getElementById('output');
            outputElement.innerHTML += 'Unknown command: ' + UTF8ToString($0) + '<br>';
        }, command.c_str());
    }
}

void main_loop() {
    char input[256];
    std::cout << ">> ";
    std::cin.getline(input, 256);
    std::string command(input);

    execute_command(command);
}

int main() {
    // Initialize root directory
    root->children[".."] = root;

    std::cout << "WebAssembly Terminal Emulator\n";
    std::cout << "Type 'exit' to quit.\n\n";

    emscripten_set_main_loop(main_loop, 0, 1);
    return 0;
}
