#ifndef CONTAINERINFO_H
#define CONTAINERINFO_H

/**
 * Liest die Gesamtzahl der Prozesse im Container aus, indem /proc
 * durchsucht wird und nur Verzeichnisse gezählt werden, deren Namen ausschließlich numerisch sind.
 *
 * @return Anzahl der Prozesse oder -1 im Fehlerfall.
 */
int get_container_process_count(void);

/**
 * Liest die Anzahl der CPU-Kerne im Container anhand von /proc/cpuinfo.
 *
 * @return Anzahl der CPU-Kerne oder -1 im Fehlerfall.
 */
int get_container_core_count(void);

/**
 * Erzeugt einen JSON-String mit container-spezifischen Kennzahlen.
 *
 * Liefert unter anderem:
 *  - processes: Gesamtzahl der Prozesse
 *  - cpu_usage: CPU-Auslastung in Prozent (basierend auf cpuacct.usage)
 *  - nprocs: Anzahl der CPU-Kerne
 *  - load_avg: Objekt mit den 1min-, 5min- und 15min-Load Average-Werten (direkt aus /proc/loadavg)
 *  - load_percent: Objekt mit den prozentualen Lastwerten (Load Average geteilt durch nprocs × 100)
 *  - virt: "lxc", wenn systemd-detect-virt "lxc" meldet, sonst "none"
 *
 * Der Aufrufer muss den zurückgegebenen Speicher mittels free() wieder freigeben.
 *
 * @return Pointer auf den JSON-String oder NULL im Fehlerfall.
 */
char *api_container_info(void);

#endif // CONTAINERINFO_H
