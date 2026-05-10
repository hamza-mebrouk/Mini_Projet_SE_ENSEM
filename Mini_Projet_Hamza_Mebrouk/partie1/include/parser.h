#ifndef PARSER_H
#define PARSER_H

#include "process.h"

/*
 * Format du fichier de configuration :
 *
 *   # commentaire
 *   PROCESS <nom> <date_arrivée> [priorité]
 *   CPU <durée>
 *   IO  <durée>
 *   CPU <durée>
 *   ...
 *   END
 *
 * Les lignes vides et les lignes commençant par '#' sont ignorées.
 * La priorité est optionnelle (défaut = 0).
 */

int parse_config(const char *filename, ProcessSet *ps);

#endif /* PARSER_H */
