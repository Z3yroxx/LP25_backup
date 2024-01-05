#include <defines.h>
#include <string.h>

/*!
 * @brief concat_path concatenates suffix to prefix into result
 * It checks if prefix ends by / and adds this token if necessary
 * It also checks that result will fit into PATH_SIZE length
 * @param result the result of the concatenation
 * @param prefix the first part of the resulting path
 * @param suffix the second part of the resulting path
 * @return a pointer to the resulting path, NULL when concatenation failed
 */
char *concat_path(char *result, char *prefix, char *suffix) {
    // Vérification des pointeurs non nuls
    if (result == NULL || prefix == NULL || suffix == NULL) {
        return NULL;
    }

    // Longueurs des chaînes de préfixe et de suffixe
    size_t prefix_len = strlen(prefix);
    size_t suffix_len = strlen(suffix);

    // Vérification de la longueur totale du chemin résultant
    if (prefix_len + suffix_len + 2 > PATH_SIZE) {
        return NULL; // La taille du résultat dépasse PATH_SIZE
    }

    // Vérification si le préfixe se termine par un séparateur de chemin '/'
    if (prefix[prefix_len - 1] == '/') {
        snprintf(result, PATH_SIZE, "%s%s", prefix, suffix);
    } else {
        snprintf(result, PATH_SIZE, "%s/%s", prefix, suffix);
    }

    return result;
}
