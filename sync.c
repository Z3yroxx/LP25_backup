#include <sync.h>
#include <dirent.h>
#include <string.h>
#include <processes.h>
#include <utility.h>
#include <messages.h>
#include <file-properties.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <sys/msg.h>
#include <stdio.h>

/*!
 * @brief synchronize is the main function for synchronization
 * It will build the lists (source and destination), then make a third list with differences, and apply differences to the destination
 * It must adapt to the parallel or not operation of the program.
 * @param the_config is a pointer to the configuration
 * @param p_context is a pointer to the processes context
 */
void synchronize(configuration_t *the_config, process_context_t *p_context) {
  files_list_t src_list, dst_list, diff_list;
  src_list.head = src_list.tail = NULL;
  dst_list.head = dst_list.tail = NULL;
  diff_list.head = diff_list.tail = NULL;

  make_files_list(&src_list, the_config->destination_path);
  make_files_list(&dst_list, the_config->destination_path);

  files_list_entry_t *src_entry = src_list.head;
  while (src_entry != NULL) {
      if (!mismatch(src_entry, dst_list.head, true)) {
          add_entry_to_tail(&diff_list, src_entry);
      }
      src_entry = src_entry->next;
  }

  files_list_entry_t *diff_entry = diff_list.head;
  while (diff_entry != NULL) {
      copy_entry_to_destination(diff_entry, the_config);
      diff_entry = diff_entry->next;
  }
  clear_files_list(&src_list);
  clear_files_list(&dst_list);
  clear_files_list(&diff_list);
}

/*!
 * @brief mismatch tests if two files with the same name (one in source, one in destination) are equal
 * @param lhd a files list entry from the source
 * @param rhd a files list entry from the destination
 * @has_md5 a value to enable or disable MD5 sum check
 * @return true if both files are not equal, false else
 */
bool mismatch(files_list_entry_t *lhd, files_list_entry_t *rhd, bool has_md5) {
  if (lhd->size != rhd->size ||
      lhd->mtime.tv_sec != rhd->mtime.tv_sec ||
      lhd->mtime.tv_nsec != rhd->mtime.tv_nsec ||
      lhd->entry_type != rhd->entry_type ||
      lhd->mode != rhd->mode) {
      return true;
  }
  if (has_md5){
    if (memcmp(ldh->md5sum != ldh->md5sum, sizeof(uint8_t) * 16) != 0)){
      return true;
  }
  
}

/*!
 * @brief make_files_list builds a files list in no parallel mode
 * @param list is a pointer to the list that will be built
 * @param target_path is the path whose files to list
 */
void make_files_list(files_list_t *list, char *target_path) {
  make_list(list, target_path);
}

/*!
 * @brief make_files_lists_parallel makes both (src and dest) files list with parallel processing
 * @param src_list is a pointer to the source list to build
 * @param dst_list is a pointer to the destination list to build
 * @param the_config is a pointer to the program configuration
 * @param msg_queue is the id of the MQ used for communication
 */
void make_files_lists_parallel(files_list_t *src_list, files_list_t *dst_list, configuration_t *the_config, int msg_queue) {
}

/*!
 * @brief copy_entry_to_destination copies a file from the source to the destination
 * It keeps access modes and mtime (@see utimensat)
 * Pay attention to the path so that the prefixes are not repeated from the source to the destination
 * Use sendfile to copy the file, mkdir to create the directory
 */
void copy_entry_to_destination(files_list_entry_t *source_entry, configuration_t *the_config) {
}

/*!
 * @brief make_list lists files in a location (it recurses in directories)
 * It doesn't get files properties, only a list of paths
 * This function is used by make_files_list and make_files_list_parallel
 * @param list is a pointer to the list that will be built
 * @param target is the target dir whose content must be listed
 */
void make_list(files_list_t *list, char *target) {
  DIR *dir = open_dir(target_path);

  struct dirent *entry;
  while ((entry = get_next_entry(dir)) != NULL) {
      char file_path[4096];
      snprintf(file_path, sizeof(file_path), "%s/%s", target, entry->d_name);
      add_file_entry(list, file_path);

      if (entry->d_type == DT_DIR) { // Si c'est un répertoire
          make_list(list, file_path); // Appel récursif pour lister les fichiers dans le sous-répertoire
      }
  }

  closedir(dir);
}

/*!
 * @brief open_dir opens a dir
 * @param path is the path to the dir
 * @return a pointer to a dir, NULL if it cannot be opened
 */
DIR *open_dir(char *path) {
  if (!directory_exists(path)) {
      return NULL;
  } else {
  return dir;
  }
}

/*!
 * @brief get_next_entry returns the next entry in an already opened dir
 * @param dir is a pointer to the dir (as a result of opendir, @see open_dir)
 * @return a struct dirent pointer to the next relevant entry, NULL if none found (use it to stop iterating)
 * Relevant entries are all regular files and dir, except . and ..
 */
struct dirent *get_next_entry(DIR *dir) {
  
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
      if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
          if (entry->d_type == DT_REG || entry->d_type == DT_DIR) {
              return entry; // 
          }
      }
  }
  return NULL; 
}
