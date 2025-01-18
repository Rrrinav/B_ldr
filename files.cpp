#include "./b_ldr.hpp"  // Include the header file containing your API

int main(int argc, char *argv[])
{
  // Initialize logging
  bld::log(bld::Log_type::INFO, "Starting file API test...");

  // Test file creation and writing
  std::string test_file = "test_file.txt";
  std::string content = "Hello, this is a test file!";
  if (bld::write_entire_file(test_file, content))
    bld::log(bld::Log_type::INFO, "File written successfully: " + test_file);
  else
    bld::log(bld::Log_type::ERROR, "Failed to write file: " + test_file);

  // Test file reading
  std::string read_content;
  if (bld::read_file(test_file, read_content))
    bld::log(bld::Log_type::INFO, "File content: " + read_content);
  else
    bld::log(bld::Log_type::ERROR, "Failed to read file: " + test_file);

  // Test file appending
  std::string append_content = "\nThis is an appended line.";
  if (bld::append_file(test_file, append_content))
    bld::log(bld::Log_type::INFO, "Content appended to file: " + test_file);
  else
    bld::log(bld::Log_type::ERROR, "Failed to append to file: " + test_file);

  // Test file copying
  std::string copied_file = "copied_file.txt";
  if (bld::copy_file(test_file, copied_file, true))
    bld::log(bld::Log_type::INFO, "File copied successfully: " + copied_file);
  else
    bld::log(bld::Log_type::ERROR, "Failed to copy file: " + test_file);

  // Test file moving
  std::string moved_file = "moved_file.txt";
  if (bld::move_file(copied_file, moved_file))
    bld::log(bld::Log_type::INFO, "File moved successfully: " + moved_file);
  else
    bld::log(bld::Log_type::ERROR, "Failed to move file: " + copied_file);

  // Test directory creation
  std::string test_dir = "test_dir";
  if (bld::create_dir_if_not_exists(test_dir))
    bld::log(bld::Log_type::INFO, "Directory created or already exists: " + test_dir);
  else
    bld::log(bld::Log_type::ERROR, "Failed to create directory: " + test_dir);

  // Test listing files in a directory
  std::vector<std::string> files = bld::list_files_in_dir(".", false);
  bld::log(bld::Log_type::INFO, "Files in current directory:");
  for (const auto &file : files)
    bld::log(bld::Log_type::INFO, "  " + file);

  // Test listing directories
  std::vector<std::string> directories = bld::list_directories(".", false);
  bld::log(bld::Log_type::INFO, "Directories in current directory:");
  for (const auto &dir : directories)
    bld::log(bld::Log_type::INFO, "  " + dir);

  // Test file replacement
  std::string replace_from = "Hello";
  std::string replace_to = "Hi";
  if (bld::replace_in_file(test_file, replace_from, replace_to))
    bld::log(bld::Log_type::INFO, "Text replaced in file: " + test_file);
  else
    bld::log(bld::Log_type::ERROR, "Failed to replace text in file: " + test_file);

  // Test reading lines from a file
  std::vector<std::string> lines;
  if (bld::read_lines(test_file, lines))
  {
    bld::log(bld::Log_type::INFO, "Lines read from file: " + test_file);
    for (const auto &line : lines)
      bld::log(bld::Log_type::INFO, "  " + line);
  }
  else
  {
    bld::log(bld::Log_type::ERROR, "Failed to read lines from file: " + test_file);
  }

  // Test directory removal
  if (bld::remove_dir(test_dir))
    bld::log(bld::Log_type::INFO, "Directory removed: " + test_dir);
  else
    bld::log(bld::Log_type::ERROR, "Failed to remove directory: " + test_dir);

  // Clean up test files
  if (bld::remove_dir(test_file))
    bld::log(bld::Log_type::INFO, "Test file removed: " + test_file);
  else
    bld::log(bld::Log_type::ERROR, "Failed to remove test file: " + test_file);

  if (bld::remove_dir(moved_file))
    bld::log(bld::Log_type::INFO, "Moved file removed: " + moved_file);
  else
    bld::log(bld::Log_type::ERROR, "Failed to remove moved file: " + moved_file);

  bld::log(bld::Log_type::INFO, "File API test completed.");
  return 0;
}
