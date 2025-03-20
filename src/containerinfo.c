#include "containerinfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/* 
   Liest alle numerischen Einträge in /proc – das entspricht den Prozessverzeichnissen.
*/
int get_container_process_count(void) {
    int count = 0;
    DIR *proc = opendir("/proc");
    if (!proc) return -1;
    struct dirent *entry;
    while ((entry = readdir(proc)) != NULL) {
        if (entry->d_type == DT_DIR) {
            int is_number = 1;
            for (char *p = entry->d_name; *p; p++) {
                if (!isdigit(*p)) { is_number = 0; break; }
            }
            if (is_number)
                count++;
        }
    }
    closedir(proc);
    return count;
}

/*
   Liest /proc/cpuinfo und zählt die Zeilen, die mit "processor" beginnen.
   Das entspricht der Anzahl der CPU-Kerne.
*/
int get_container_core_count(void) {
    int count = 0;
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) return -1;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "processor", 9) == 0)
            count++;
    }
    fclose(fp);
    return count;
}

/*
   Diese Funktion liest die kumulierte CPU-Zeit (in Nanosekunden) aus der cgroup-Datei
   und berechnet anhand der Differenz zur vorherigen Messung einen CPU-Auslastungswert in Prozent.
   Da der Wert für alle Kerne zusammen erfasst wird, wird er durch (Anzahl der Kerne * 1e9) geteilt.
*/
static double get_container_cpu_usage_percentage(void) {
    unsigned long long current_usage = 0;
    FILE *fp = fopen("/sys/fs/cgroup/cpu,cpuacct/cpuacct.usage", "r");
    if (fp == NULL) {
        fp = fopen("/sys/fs/cgroup/cpuacct/cpuacct.usage", "r");
        if (fp == NULL)
            return -1;
    }
    if (fscanf(fp, "%llu", &current_usage) != 1) {
        fclose(fp);
        return -1;
    }
    fclose(fp);
    static unsigned long long last_usage = 0;
    static time_t last_cpu_time = 0;
    time_t now = time(NULL);
    double dt = (last_cpu_time == 0) ? 0 : difftime(now, last_cpu_time);
    double usage_percent = 0;
    int cores = get_container_core_count();
    if (last_cpu_time != 0 && dt > 0 && cores > 0) {
        unsigned long long delta = current_usage - last_usage;
        usage_percent = (delta / (dt * 1e9 * cores)) * 100.0;
    }
    last_usage = current_usage;
    last_cpu_time = now;
    return usage_percent;
}

/*
   Prüft mittels systemd-detect-virt, ob Pi‑hole in einem LXC-Container läuft.
*/
static int is_lxc(void) {
    FILE *fp = popen("systemd-detect-virt", "r");
    if (!fp)
        return 0;
    char output[128] = {0};
    if (fgets(output, sizeof(output), fp) != NULL) {
        pclose(fp);
        output[strcspn(output, "\n")] = '\0';
        return (strcmp(output, "lxc") == 0);
    }
    pclose(fp);
    return 0;
}

/*
   Erzeugt einen JSON-String mit container-spezifischen Kennzahlen.
   Die Load Average-Werte werden direkt aus /proc/loadavg gelesen – dank LXCFS liefert dieser Pfad
   container-spezifische Werte.
*/
char *api_container_info(void) {
    int proc_count = get_container_process_count();
    int nprocs = get_container_core_count();
    double cpu_usage = get_container_cpu_usage_percentage(); // in %

    /* Lade die Load Average-Werte aus /proc/loadavg */
    double la1 = 0.0, la5 = 0.0, la15 = 0.0;
    FILE *fp = fopen("/proc/loadavg", "r");
    if (fp) {
        if (fscanf(fp, "%lf %lf %lf", &la1, &la5, &la15) != 3) {
            la1 = la5 = la15 = 0.0;
        }
        fclose(fp);
    }
    /* Berechne die prozentuale Last: Ein Load Average von 1.0 pro Kern entspricht 100% */
    double load_percent_1 = (nprocs > 0) ? (la1 / nprocs * 100.0) : 0;
    double load_percent_5 = (nprocs > 0) ? (la5 / nprocs * 100.0) : 0;
    double load_percent_15 = (nprocs > 0) ? (la15 / nprocs * 100.0) : 0;

    int container_flag = is_lxc();

    char *json = malloc(4096);
    if (!json) return NULL;
    snprintf(json, 4096,
        "{\"processes\": %d, \"cpu_usage\": \"%.2f%%\", \"nprocs\": %d, "
        "\"load_avg\": {\"1min\": \"%.2f\",\"5min\": \"%.2f\",\"15min\": \"%.2f\"}, "
        "\"load_percent\": {\"1min\": \"%.2f%%\", \"5min\": \"%.2f%%\", \"15min\": \"%.2f%%\"}, "
        "\"virt\": \"%s\"}",
        proc_count, cpu_usage, nprocs,
        la1, la5, la15,
        load_percent_1, load_percent_5, load_percent_15,
        container_flag ? "lxc" : "none");
    return json;
}
