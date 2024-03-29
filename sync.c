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
    if (p_context == NULL) {
        printf("Paramètres invalides\n");
        exit(-1);
    }

    // Initialisation des listes source et destination
    files_list_t source, destination, difference;
    source.head = source.tail = NULL;
    destination.head = destination.tail = NULL;
    difference.head = difference.tail = NULL;

    // Création des listes de fichiers en fonction du mode de synchronisation
    if (!the_config->is_parallel) {
        make_files_list(&source, the_config->source);
        make_files_list(&destination, the_config->destination);
    } else {
        make_files_lists_parallel(&source, &destination, the_config, p_context->message_queue_id);
    }

    // Affichage des fichiers source et destination
    display_files_list(&source);
    display_files_list(&destination);

    files_list_entry_t *tmp = source.head;

    // Comparaison des fichiers source et destination
    while (tmp != NULL) {
        size_t start_of_src = strlen(the_config->source) + 1;
        size_t start_of_dest = strlen(the_config->destination) + 1;

        files_list_entry_t *result = find_entry_by_name(&destination, tmp->path_and_name, start_of_src, start_of_dest);

        if (result == NULL || mismatch(tmp, result, the_config->uses_md5)) {

            // Ajout des fichiers différents à la liste de différences
            files_list_entry_t *tmp_copy = malloc(sizeof(files_list_entry_t));
            if (tmp_copy == NULL) {
                printf("Erreur d'allocation mémoire\n");
                exit(-1);
            }
            memcpy(tmp_copy, tmp, sizeof(files_list_entry_t));
            tmp_copy->next = NULL;
            add_entry_to_tail(&difference, tmp_copy);
        } else {
            printf("\nLes fichiers sont identiques\n");
        }

        tmp = tmp->next;
    }

    // Copie des fichiers de la liste de différences vers la destination
    files_list_entry_t *tmp_dif = difference.head;
    while (tmp_dif != NULL) {
        copy_entry_to_destination(tmp_dif, the_config);
        tmp_dif = tmp_dif->next;
    }

    // Nettoyage des listes de fichiers
    clear_files_list(&difference);
    clear_files_list(&source);
    clear_files_list(&destination);
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
    // Vérifie si les paramètres sont valides
    if (src_list == NULL || dst_list == NULL || the_config == NULL) {
        printf("Paramètres invalides\n");
        exit(-1);
    }

    fflush(stdout);

    // Envoi des commandes d'analyse de répertoire pour le source et la destination
    send_analyze_dir_command(msg_queue, COMMAND_CODE_ANALYZE_DIR, the_config->source);
    send_analyze_dir_command(msg_queue, COMMAND_CODE_ANALYZE_DIR, the_config->destination);

    bool source_loop = true;
    bool destination_loop = true;

    any_message_t source_response, destination_response, src_end, dst_end;

    // Boucle pour recevoir les réponses des analyseurs en parallèle
    do {
        receive_messages(msg_queue, COMMAND_CODE_FILE_ENTRY, &source_response);
        receive_messages(msg_queue, COMMAND_CODE_FILE_ENTRY, &destination_response);
        receive_messages(msg_queue, COMMAND_CODE_FILE_ANALYZED, &src_end);
        receive_messages(msg_queue, COMMAND_CODE_FILE_ANALYZED, &dst_end);

        // Processus pour gérer les réponses des analyseurs
        if (source_response.list_entry.op_code == COMMAND_CODE_ANALYZE_FILE) {
            // Si c'est une réponse à l'analyse de fichier source, l'ajouter à src_list
            files_list_entry_t *tmp_copy = malloc(sizeof(files_list_entry_t));
            if (tmp_copy == NULL) {
                printf("Erreur d'allocation mémoire\n");
                exit(-1);
            }
            memcpy(tmp_copy, &source_response.list_entry.payload, sizeof(files_list_entry_t));
            add_entry_to_tail(src_list, tmp_copy);
        }

        if (destination_response.list_entry.op_code == COMMAND_CODE_ANALYZE_FILE) {
            // Si c'est une réponse à l'analyse de fichier destination, l'ajouter à dst_list
            files_list_entry_t *tmp_copy = malloc(sizeof(files_list_entry_t));
            if (tmp_copy == NULL) {
                printf("Erreur d'allocation mémoire\n");
                exit(-1);
            }
            memcpy(tmp_copy, &destination_response.list_entry.payload, sizeof(files_list_entry_t));
            add_entry_to_tail(dst_list, tmp_copy);
        }

        if (src_end.simple_command.message == COMMAND_CODE_LIST_COMPLETE) {
            printf("Fin_source\n");
            source_loop = false;
        }

        if (dst_end.simple_command.message == COMMAND_CODE_LIST_COMPLETE) {
            printf("Fin_destination\n");
            destination_loop = false;
        }

    } while (source_loop || destination_loop);

    // Envoi de la confirmation de terminaison
    int result = send_terminate_confirm(msg_queue, COMMAND_CODE_TERMINATE_OK);
    if (result == -1) {
        fprintf(stderr, "Erreur dans l'envoi du message de terminaison\n");
        exit(-1);
    }
}

void copy_entry_to_destination(files_list_entry_t *source_entry, configuration_t *the_config) {
    // Vérifie si les paramètres passés sont valides
    if (source_entry == NULL || the_config == NULL) {
        printf("Paramètres invalides\n");
        return;
    }

    // Déclare et initialise les chemins source et destination avec les chemins du fichier source et de destination respectivement
    char source_path[1024];
    char dest_path[1024];
    strcpy(source_path, the_config->source);
    strcpy(dest_path, the_config->destination);

    // Vérifie si l'entrée est un dossier
    if (source_entry->entry_type == DOSSIER){
        char directory[PATH_SIZE];
        // Construit le chemin du dossier à créer dans la destination
        concat_path(directory, dest_path, source_entry->path_and_name + strlen(the_config->source) + 1);
        // Crée le dossier avec les permissions spécifiées dans source_entry->mode
        if (mkdir(directory, source_entry->mode) != 0) {
            perror("Erreur dans la création du dossier");
            return;
        }
    }
    else { // Si l'entrée est un fichier
        char source_file_path[PATH_SIZE];
        char destination_file_path[PATH_SIZE];
        // Construit les chemins complets des fichiers source et destination
        concat_path(source_file_path, source_path, source_entry->path_and_name);
        concat_path(destination_file_path, dest_path, source_entry->path_and_name + strlen(the_config->source) + 1);

        // Ouvre le fichier source en lecture seule
        int source_fd = open(source_file_path, O_RDONLY);
        if (source_fd == -1) {
            perror("Erreur dans l'ouverture du fichier");
            return;
        }

        // Ouvre ou crée le fichier destination avec les permissions spécifiées dans source_entry->mode
        int dest_fd = open(destination_file_path, O_WRONLY | O_CREAT | O_TRUNC, source_entry->mode);
        if (dest_fd == -1) {
            perror("Erreur dans l'ouverture ou dans la création du fichier");
            close(source_fd);
            return;
        }

        off_t offset = 0;
        // Copie le contenu du fichier source vers le fichier destination en utilisant sendfile
        off_t bytes_sent = sendfile(dest_fd, source_fd, &offset, source_entry->size);
        if (bytes_sent == -1) {
            perror("Erreur dans la copie du fichier");
        }

        // Ferme les descripteurs de fichier
        close(source_fd);
        close(dest_fd);
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
  if (list == NULL || target == NULL) {
    printf("Paramètres invalides\n");
    return;
  }

  // Vérification de l'existence du répertoire cible
  if (!directory_exists(target)) {
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




