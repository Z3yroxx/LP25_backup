#include <configuration.h>
#include <stddef.h>
#include <stdlib.h>
#include <getopt.h>
#include <stdio.h>
#include <string.h>

typedef enum {DATE_SIZE_ONLY, NO_PARALLEL} long_opt_values;

/*!
 * @brief function display_help displays a brief manual for the program usage
 * @param my_name is the name of the binary file
 * This function is provided with its code, you don't have to implement nor modify it.
 */
void display_help(char *my_name) {
    printf("%s [options] source_dir destination_dir\n", my_name);
    printf("Options: \t-n <processes count>\tnumber of processes for file calculations\n");
    printf("         \t-h display help (this text)\n");
    printf("         \t--date_size_only disables MD5 calculation for files\n");
    printf("         \t--no-parallel disables parallel computing (cancels values of option -n)\n");
    printf("         \t--dry-run lists the changes that would need to be synchronized but doesn't perform them\n");
    printf("         \t-v enables verbose mode\n");
}

/*!
 * @brief init_configuration initializes the configuration with default values
 * @param the_config is a pointer to the configuration to be initialized
 */
void init_configuration(configuration_t *the_config) {
    strcpy(the_config->source, "src_default");
    strcpy(the_config->destination, "dst_default");
    the_config->processes_count = 1;
    the_config->is_parallel = false;
    the_config->uses_md5 = false;
    the_config->is_verbose = false;
    the_config->is_dry_run = false;
}

/*!
 * @brief set_configuration updates a configuration based on options and parameters passed to the program CLI
 * @param the_config is a pointer to the configuration to update
 * @param argc is the number of arguments to be processed
 * @param argv is an array of strings with the program parameters
 * @return -1 if configuration cannot succeed, 0 when ok
 */
int set_configuration(configuration_t *the_config, int argc, char *argv[]) {
    
     if (the_config == NULL || argc < 3) {
        return -1;  // Paramètres invalides
    }

    // Initialiser la configuration avec des valeurs par défaut
    init_configuration(the_config);

    int option;
    const char *short_options = "n:hvd";
    const struct option long_options[] = {
            {"date_size_only", no_argument, NULL, 'x'},
            {"no-parallel", no_argument, NULL, 'y'},
            {"dry-run", no_argument, NULL, 'd'},
            {"verbose", no_argument, NULL, 'v'},
            {NULL, 0, NULL, 0}};

    while ((option = getopt_long(argc, argv, short_options, long_options, NULL)) != -1) {
        switch (option) {
            case 'n':
                the_config->processes_count = (uint8_t)atoi(optarg);
                break;
            case 'h':
                display_help(argv[0]);
                return -1; // Terminer le programme après affichage de l'aide
            case 'x':
                the_config->uses_md5 = false;
                break;
            case 'y':
                the_config->is_parallel = false;
                the_config->processes_count = 1; // Réinitialiser le nombre de processus
                break;
            case 'd':
                the_config->is_dry_run = true;
                break;
            case 'v':
                the_config->is_verbose = true;
                break;
            default:
                return -1; // Option non reconnue
        }
    }

    // Mettre à jour les champs source et destination
    strncpy(the_config->source, argv[optind], sizeof(the_config->source) - 1);
    the_config->source[sizeof(the_config->source) - 1] = '\0';

    strncpy(the_config->destination, argv[optind + 1], sizeof(the_config->destination) - 1);
    the_config->destination[sizeof(the_config->destination) - 1] = '\0';

    return 0; // Succès
}
