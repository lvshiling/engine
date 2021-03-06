/**
 * @file
 */

#pragma once

#include <memory>
#include "File.h"
#include <string>
#include <stack>
#include <vector>
#include <uv.h>
#include <unordered_map>
#include <stdarg.h>
#include <SDL_stdinc.h>

namespace io {

typedef void (*FileWatcher)(const char* file);

/**
 * @brief Hide platform specific details about the io handling for files.
 *
 * @note You can load file synchronous or asynchronous with a callback.
 */
class Filesystem {
private:
	std::string _organisation;
	std::string _appname;

	/**
	 * This is the directory where the application was run from, which is probably
	 * the installation directory or the current working directory. In case the
	 * binary is a symlink, it it resolved.
	 */
	std::string _basePath;
	std::string _homePath;
	std::vector<std::string> _paths;

	std::stack<std::string> _dirStack;
	std::unordered_map<std::string, uv_fs_event_t*> _watches;
	uv_loop_t *_loop = nullptr;

public:
	~Filesystem();

	bool init(const std::string& organisation, const std::string& appname);
	void shutdown();

	void update();

	bool registerPath(const std::string& path);

	bool unwatch(const std::string& path);
	bool unwatch(const io::FilePtr& file);
	bool watch(const std::string& path, FileWatcher watcher);
	bool watch(const io::FilePtr& file, FileWatcher watcher);

	/**
	 * @brief Get the path where the application resides.
	 *
	 * Get the "base path". This is the directory where the application was run
	 * from, which is probably the installation directory, and may or may not
	 * be the process's current working directory.
	 */
	const std::string& basePath() const;
	/**
	 * @brief The path where the application can store data
	 */
	const std::string& homePath() const;

	/**
	 * @brief Returns a path where the given file can be saved.
	 */
	const std::string writePath(const char* name) const;

	bool exists(const std::string& filename) const;

	struct DirEntry {
		std::string name;
		enum class Type : uint8_t {
			file,
			dir,
			unknown
		};
		Type type;
		uint64_t size;
	};

	bool list(const std::string& directory, std::vector<DirEntry>& entities) const;
	bool list(const std::string& directory, std::vector<DirEntry>& entities, const std::string& filter) const;

	static bool isReadableDir(const std::string& name);
	static bool isRelativePath(const std::string& name);

	static std::string absolutePath(const std::string& path);

	/**
	 * @brief Changes the current working directory
	 * @see popDir()
	 * @see pushDir()
	 */
	static bool chdir(const std::string& directory);

	/**
	 * @brief changes the current working dir to the last pushed one
	 */
	bool popDir();

	/**
	 * @brief Push a working dir change onto the stack for later returning without knowing the origin
	 */
	bool pushDir(const std::string& directory);

	io::FilePtr open(const std::string& filename, FileMode mode = FileMode::Read) const;

	std::string load(SDL_PRINTF_FORMAT_STRING const char *filename, ...) SDL_PRINTF_VARARG_FUNC(2);

	std::string load(const std::string& filename) const;

	bool write(const std::string& filename, const uint8_t* content, size_t length);

	bool write(const std::string& filename, const std::string& string);

	/**
	 * @note The difference to the usual write() methods is that the given path is not put into the
	 * known file system structure of the application. It just uses the given name.
	 */
	bool syswrite(const std::string& filename, const uint8_t* content, size_t length) const;

	/**
	 * @note The difference to the usual write() methods is that the given path is not put into the
	 * known file system structure of the application. It just uses the given name.
	 */
	bool syswrite(const std::string& filename, const std::string& string) const;

	bool createDir(const std::string& dir, bool recursive = true) const;

	bool removeDir(const std::string& dir, bool recursive = false) const;
	bool removeFile(const std::string& file) const;
private:
	static bool _list(const std::string& directory, std::vector<DirEntry>& entities);
	static bool _list(const std::string& directory, std::vector<DirEntry>& entities, const std::string& filter);
};

inline bool Filesystem::exists(const std::string& filename) const {
	return open(filename)->exists();
}

inline const std::string& Filesystem::basePath() const {
	return _basePath;
}

inline const std::string& Filesystem::homePath() const {
	return _homePath;
}

typedef std::shared_ptr<Filesystem> FilesystemPtr;

}
