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
 * @brief La fonction synchronize est la fonction principale de synchronisation.
 * Elle construit les listes (source et destination), puis crée une troisième liste avec les différences, et applique ces différences à la destination.
 * Elle doit s'adapter à l'opération parallèle ou non du programme.
 * @param the_config est un pointeur vers la configuration
 * @param p_context est un pointeur vers le contexte des processus
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
      if (!mismatch(src_entry, dst_list.head, the_config->uses_md5)) {
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
 * @brief La fonction mismatch teste si deux fichiers avec le même nom (un dans la source, un dans la destination) sont égaux.
 * @param lhd est une entrée de liste de fichiers provenant de la source
 * @param rhd est une entrée de liste de fichiers provenant de la destination
 * @has_md5 est une valeur pour activer ou désactiver la vérification de la somme de contrôle MD5
 * @return vrai si les deux fichiers ne sont pas égaux, faux sinon
 */
bool mismatch(files_list_entry_t *lhd, files_list_entry_t *rhd, bool has_md5) {
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
 * @brief La fonction make_files_list construit une liste de fichiers en mode non-parallèle.
 * @param list est un pointeur vers la liste qui sera construite
 * @param target_path est le chemin dont les fichiers doivent être listés
 */
void make_files_list(files_list_t *list, char *target_path) {
  // Appel de la fonction pour construire la liste de fichiers de manière séquentielle
  make_list(list, target_path);
}

/*!
 * @brief La fonction make_files_lists_parallel crée les listes de fichiers source et destination avec un traitement parallèle.
 * @param src_list est un pointeur vers la liste source à construire
 * @param dst_list est un pointeur vers la liste de destination à construire
 * @param the_config est un pointeur vers la configuration du programme
 * @param msg_queue est l'identifiant de la MQ utilisée pour la communication
 */
void make_files_lists_parallel(files_list_t *src_list, files_list_t *dst_list, configuration_t *the_config, int msg_queue) {
  // À implémenter - fonction pour construire les listes en parallèle
  // Cette fonction pourrait utiliser des threads ou d'autres mécanismes de parallélisation
}

/*!
 * @brief La fonction copy_entry_to_destination copie un fichier de la source vers la destination.
 * Elle conserve les modes d'accès et mtime (@voir utimensat)
 * Attention au chemin pour éviter que les préfixes ne se répètent de la source à la destination.
 * Utilise sendfile pour copier le fichier et mkdir pour créer le répertoire
 */
void copy_entry_to_destination(files_list_entry_t *source_entry, configuration_t *the_config) {
  // Chemins de la source et de la destination pour le fichier à copier
  char source_path[4096];
  char destination_path[4096];

  // Construction des chemins complets de la source et de la destination
  snprintf(source_path, sizeof(source_path), "%s/%s", the_config->source, source_entry->path_and_name);
  snprintf(destination_path, sizeof(destination_path), "%s/%s", the_config->destination, source_entry->path_and_name);

  // Vérification si l'entrée est un dossier
  if (source_entry->entry_type == DOSSIER) {
      // Création du répertoire de destination s'il n'existe pas déjà
      if (mkdir(destination_path, 0777) != 0 && errno != EEXIST) {
          perror("Erreur lors de la création du répertoire destination");
          return;
      }
      return;
  }

  // Ouverture des fichiers source et destination
  int source_fd = open(source_path, O_RDONLY);
  int destination_fd = open(destination_path, O_WRONLY | O_CREAT | O_TRUNC, 0777);

  // Vérification des erreurs lors de l'ouverture des fichiers
  if (source_fd == -1 || destination_fd == -1) {
      perror("Erreur lors de l'ouverture des fichiers");
      if (source_fd != -1) close(source_fd);
      if (destination_fd != -1) close(destination_fd);
      return;
  }

  // Obtention des informations sur le fichier source
  struct stat source_stat;
  fstat(source_fd, &source_stat);

  // Copie du fichier de la source vers la destination en utilisant sendfile
  off_t offset = 0;
  ssize_t bytes_copied = sendfile(destination_fd, source_fd, &offset, source_stat.st_size);
  if (bytes_copied == -1) {
      perror("Erreur lors de la copie du fichier");
      close(source_fd);
      close(destination_fd);
      return;
  }

  // Fermeture des fichiers source et destination
  close(source_fd);
  close(destination_fd);

  // Mise à jour des timestamps (mtime) de la destination pour correspondre à ceux de la source
  struct timespec times[2] = {source_stat.st_atim, source_stat.st_mtim};
  utimensat(AT_FDCWD, destination_path, times, 0);
}

/*!
 * @brief La fonction make_list liste les fichiers dans un emplacement (elle récursent dans les répertoires).
 * Elle ne récupère pas les propriétés des fichiers, seulement une liste de chemins.
 * Cette fonction est utilisée par make_files_list et make_files_lists_parallel.
 * @param list est un pointeur vers la liste qui sera construite
 * @param target est le répertoire dont le contenu doit être listé
 */
void make_list(files_list_t *list, char *target) {
  // Ouverture du répertoire cible
  DIR *dir = open_dir(target);

  // Récupération des entrées du répertoire
  struct dirent *entry;
  while ((entry = get_next_entry(dir)) != NULL) {
      // Construction du chemin complet du fichier
      char file_path[4096];
      snprintf(file_path, sizeof(file_path), "%s/%s", target, entry->d_name);
      // Ajout du chemin à la liste
      add_file_entry(list, file_path);

      // Si l'entrée est un dossier, récursion pour lister son contenu
      if (entry->d_type == DT_DIR) { 
          make_list(list, file_path); 
      }
  }

  // Fermeture du répertoire
  closedir(dir);
