#include "FileDialog.h"
#include "tinyfiledialogs/tinyfiledialogs.h"
#include <LibSL.h>

//-------------------------------------------------------

std::string newFileDialog(const std::vector<const char*>& _filter) {
    const char* filename = tinyfd_inputBox("New File", "Enter a name <name.extension>", ".ice");

    std::string ext = extractExtension(filename);
    if (filename && (ext == "lua" || ext == "ice")) {
        return std::string(filename);
    }
    else {
        return "";
    }
}

//-------------------------------------------------------

std::string openFileDialog(const std::vector<const char*>& _filter) {
  const char* file = tinyfd_openFileDialog("Open File", nullptr, static_cast<int>(_filter.size()), _filter.data(), nullptr, 0);
  if (file) {
    return std::string(file);
  } else {
    return "";
  }
}

//-------------------------------------------------------

std::string openFileDialog(const char* _directory, const std::vector<const char*>& _filter) {
  const char* file = tinyfd_openFileDialog("Open File", _directory, static_cast<int>(_filter.size()), _filter.data(), nullptr, 0);
  if (file) {
    return std::string(file);
  }
  else {
    return "";
  }
}

//-------------------------------------------------------

std::string saveFileDialog(const char* _proposedFileNameFullPath, const std::vector<const char*>& _filter) {
  const char* file = tinyfd_saveFileDialog("Save File", _proposedFileNameFullPath, static_cast<int>(_filter.size()), _filter.data(), nullptr);
  if (file) {
    return std::string(file);
  } else {
    return "";
  }
}

//-------------------------------------------------------

std::string openFolderDialog(const char* _proposedFolderNameFullPath) {
  const char* folder = tinyfd_selectFolderDialog("Open Folder", _proposedFolderNameFullPath);
  if (folder) {
    return std::string(folder);
  } else {
    return "";
  }
}

