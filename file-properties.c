#include <file-properties.h>

#include <sys/stat.h>
#include <dirent.h>
#include <openssl/evp.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <defines.h>
#include <fcntl.h>
#include <stdio.h>
#include <utility.h>

/*!
 * @brief get_file_stats gets all of the required information for a file (inc. directories)
 * @param the files list entry
 * You must get:
 * - for files:
 *   - mode (permissions)
 *   - mtime (in nanoseconds)
 *   - size
 *   - entry type (FICHIER)
 *   - MD5 sum
 * - for directories:
 *   - mode
 *   - entry type (DOSSIER)
 * @return -1 in case of error, 0 else
 */
int get_file_stats(files_list_entry_t *entry) {
    struct stat info;

    if (entry == NULL) {
        printf("Erreur d'entrée : NULL\n");
        return -1;
    }
    
    if (stat(entry->path_and_name, &info) == -1) {
        printf("Erreur lors de la lecture des informations du fichier ou du dossier");
        return -1;
    }
    if (S_ISREG(info.st_mode)) {
        entry->mode = info.st_mode;
        entry->mtime.tv_sec = info.st_mtim.tv_sec; // Attribution du temps de modification
        entry->mtime.tv_nsec = info.st_mtim.tv_nsec; // Attribution des nanosecondes
        entry->size = info.st_size;
        entry->entry_type = FICHIER;

        if (compute_file_md5(entry) == -1) {
            return -1;
        }
        return 0;
        
    } else if (S_ISDIR(info.st_mode)) {
        entry->mode = info.st_mode;
        entry->entry_type = DOSSIER;
        return 0;
        
    } else {
        return -1;
    }
    
}

/*!
 * @brief compute_file_md5 computes a file's MD5 sum
 * @param the pointer to the files list entry
 * @return -1 in case of error, 0 else
 * Use libcrypto functions from openssl/evp.h
 */
int compute_file_md5(files_list_entry_t *entry) {
    FILE *file;
    unsigned char buffer[MD5_DIGEST_LENGTH];
    size_t bytesRead;
    unsigned char data[1024];
    MD5_CTX md5Context;

    file = fopen(entry->path_and_name, "rb");
    if (file == NULL) {
        perror("Erreur");
        return -1;
    } else if (file != NULL){

        MD5_Init(&md5Context);
    
        while ((bytesRead = fread(data, 1, sizeof(data), file)) != 0) {
            MD5_Update(&md5Context, data, bytesRead);
        }
    
        MD5_Final(buffer, &md5Context);
    
        fclose(file);
        memcpy(entry->md5sum, buffer, MD5_DIGEST_LENGTH);
    
        return 0;
    }
}

/*!
 * @brief directory_exists tests the existence of a directory
 * @path_to_dir a string with the path to the directory
 * @return true if directory exists, false else
 */
bool directory_exists(char *path_to_dir) {

    if (path_to_dir == NULL) {
        printf("Erreur répertoire : NULL\n");
        return false;
    }
    
    DIR *directory;
    directory = opendir(path_to_dir);
    if (directory == NULL) {
        printf("Erreur lors de l'ouverture du repertoire : %s\n", path_to_dir);
        return false;
    }
    closedir(directory);
    return true;
}

/*!
 * @brief is_directory_writable tests if a directory is writable
 * @param path_to_dir the path to the directory to test
 * @return true if dir is writable, false else
 * Hint: try to open a file in write mode in the target directory.
 */
bool is_directory_writable(char *path_to_dir) {
    if (access(path_to_dir, W_OK) != 0) {
        printf("Erreur : le répertoire de destination ne peut pas être modifié");
        return false;
    }
    return true;
