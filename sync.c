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
  // Initialisation des listes
  files_list_t src_list, dst_list, diff_list;
  src_list.head = src_list.tail = NULL;
  dst_list.head = dst_list.tail = NULL;
  diff_list.head = diff_list.tail = NULL;

  // Construction des listes de fichiers source et destination
  make_files_list(&src_list, the_config->source);
  make_files_list(&dst_list, the_config->destination);

  // Comparaison des listes pour détecter les différences
  files_list_entry_t *src_entry = src_list.head;
  while (src_entry != NULL) {
      if (mismatch(src_entry, dst_list.head, the_config->uses_md5)) {
          add_entry_to_tail(&diff_list, src_entry);
      }
      src_entry = src_entry->next;
  }

  // Copie des fichiers de la liste de différences vers la destination
  files_list_entry_t *diff_entry = diff_list.head;
  while (diff_entry != NULL) {
      copy_entry_to_destination(diff_entry, the_config);
      diff_entry = diff_entry->next;
  }

  // Nettoyage des listes de fichiers
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

  // Vérification des paramètres passés
  if (lhd == NULL || rhd == NULL) {
    printf("Paramètres invalides\n");
    return true; 
  }

  // Comparaison des attributs des fichiers
  if (lhd->size != rhd->size ||
      lhd->mtime.tv_sec != rhd->mtime.tv_sec ||
      lhd->mtime.tv_nsec != rhd->mtime.tv_nsec ||
      lhd->entry_type != rhd->entry_type ||
      lhd->mode != rhd->mode) {
      return true;
  }
  // Vérification de la somme de contrôle MD5 si activée
  if (has_md5) {
    if (memcmp(lhd->md5sum, rhd->md5sum, sizeof(uint8_t) * 16) != 0) {
      return true;
    }
  }
  return false;
}

/*!
 * @brief make_files_list builds a files list in no parallel mode
 * @param list is a pointer to the list that will be built
 * @param target_path is the path whose files to list
 */
void make_files_list(files_list_t *list, char *target_path) {

  // Vérification des paramètres passés
  if (list == NULL || target_path == NULL) {
    printf("Paramètres invalides\n");
    return;
  }

  // Vérification de l'existence du répertoire cible
  if (!directory_exists(target_path)) {
    return;
  }

  // Appel de la fonction pour construire la liste de fichiers
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
  // à faire pour la partie II du projet : intégration des processus et du mode en parallèle 
}

/*!
 * @brief copy_entry_to_destination copies a file from the source to the destination
 * It keeps access modes and mtime (@see utimensat)
 * Pay attention to the path so that the prefixes are not repeated from the source to the destination
 * Use sendfile to copy the file, mkdir to create the directory
 */
void copy_entry_to_destination(files_list_entry_t *source_entry, configuration_t *the_config) {
  // Vérification des paramètres passés
  if (source_entry == NULL || the_config == NULL) {
      printf("Paramètres invalides\n");
      return;
  }

  // Chemin source du fichier à copier
  const char *source_path = source_entry->path_and_name;

  // Construire le chemin de destination pour le fichier
  char destination_path[4096];
  snprintf(destination_path, sizeof(destination_path), "%s/%s", the_config->destination, source_path + strlen(the_config->source));

  // Trouver le dernier '/' dans le chemin source pour obtenir le répertoire parent
  char *last_slash = strrchr(destination_path, '/');
  if (last_slash != NULL) {
      *last_slash = '\0'; // Mettre fin au chemin au répertoire parent
      // Vérifier si le répertoire parent existe dans la destination, sinon le créer
      if (mkdir(destination_path, 0777) == -1 && errno != EEXIST) {
          perror("Erreur lors de la création du répertoire parent dans la destination");
          return;
      }
      *last_slash = '/'; // Restaurer le chemin complet
  }

  // Si c'est un répertoire, rien à copier, juste à recréer la structure
  if (source_entry->is_directory) {
      // Création du répertoire dans la destination si nécessaire
      if (mkdir(destination_path, 0777) != 0 && errno != EEXIST) {
          perror("Erreur lors de la création du répertoire dans la destination");
      }
      return;
  }

  // Copie du fichier
  int source_fd = open(source_path, O_RDONLY);
  if (source_fd == -1) {
      perror("Erreur lors de l'ouverture du fichier source");
      return;
  }

  int destination_fd = open(destination_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);
  if (destination_fd == -1) {
      perror("Erreur lors de l'ouverture du fichier destination");
      close(source_fd);
      return;
  }

  // Copie du contenu du fichier source vers le fichier destination
  ssize_t bytes_read;
  char buffer[4096];
  while ((bytes_read = read(source_fd, buffer, sizeof(buffer))) > 0) {
      ssize_t bytes_written = write(destination_fd, buffer, bytes_read);
      if (bytes_written == -1) {
          perror("Erreur lors de l'écriture dans le fichier destination");
          close(source_fd);
          close(destination_fd);
          return;
      }
  }

  // Fermeture des descripteurs de fichiers
  close(source_fd);
  close(destination_fd);

  // Mise à jour des timestamps (atime et mtime) de la destination pour correspondre à ceux de la source_entry
  struct timespec times[2];
  times[0] = source_entry->atime; // Copie des timestamps d'accès
  times[1] = source_entry->mtime; // Copie des timestamps de modification

  if (utimensat(AT_FDCWD, destination_path, times, 0) == -1) {
      perror("Erreur lors de la mise à jour des timestamps de la destination");
      return;
  }
}



/*!
 * @brief make_list lists files in a location (it recurses in directories)
 * It doesn't get files properties, only a list of paths
 * This function is used by make_files_list and make_files_list_parallel
 * @param list is a pointer to the list that will be built
 * @param target is the target dir whose content must be listed
 */
void make_list(files_list_t *list, const char *target) {

  // Vérification des paramètres passés
  if (list == NULL || target_path == NULL) {
    printf("Paramètres invalides\n");
    return;
  }

  // Vérification de l'existence du répertoire cible
  if (!directory_exists(target_path)) {
    return;
  }

  // Ouverture du répertoire cible
  DIR *dir = open_dir(target);

  // Récupération des entrées du répertoire
  struct dirent *entry;
  while ((entry = readdir(dir)) != NULL) {
      // Construction du chemin complet du fichier
      char file_path[4096];
      snprintf(file_path, sizeof(file_path), "%s/%s", target, entry->d_name);
      // Ajout du chemin à la liste chainée
      add_file_entry(list, file_path);

      // Si l'entrée est un dossier, récursion pour lister son contenu
      if (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) { 
          make_list(list, file_path); 
      }
  }

  // Fermeture du répertoire
  closedir(dir);
}


/*!
 * @brief open_dir opens a dir
 * @param path is the path to the dir
 * @return a pointer to a dir, NULL if it cannot be opened
 */
DIR *open_dir(char *path) {
  // Vérification si le répertoire existe
  if (!directory_exists(path)) {
      return NULL;
  } else {
      // Ouverture du répertoire
      DIR *dir = opendir(path);
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
    // Vérifie si le pointeur de répertoire est NULL
    if (!dir)
        return NULL;
    // Lit la première entrée du répertoire
    struct dirent *entry = readdir(dir);
    // Si aucune entrée n'est lue, renvoie NULL
    if (!entry)
        return NULL;
    // Boucle pour rechercher la prochaine entrée de répertoire valide
    while (entry) {
        // Vérifie si l'entrée est "." (répertoire courant), ".." (répertoire parent),
        // ou si elle n'est pas un répertoire (DT_DIR) ou un fichier régulier (DT_REG)
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0
            || entry->d_type != DT_DIR || entry->d_type == DT_REG) {
            // Passe à l'entrée suivante si l'entrée actuelle n'est pas valide
            entry = readdir(dir);
        } else {
            break;
        }
    }
    // Renvoie l'entrée de répertoire valide trouvée 
    return entry;
}




