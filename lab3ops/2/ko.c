#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/ktime.h> 

#define PROC_NAME "tsulab"

// Базовая дата: 18 декабря 2025, 00:00 UTC
#define BASE_TIMESTAMP 1766016000L  // 2025-12-18 00:00:00 UTC

// Расстояния на эту дату (в км)
#define BASE_V1_KM 25274000000UL
#define BASE_V2_KM 21087000000UL

// Скорости (км/с)
#define SPEED_V1 17
#define SPEED_V2 15

//Расстояние для Voyager 1 увеличится на +1 million km примерно через 16 часов 20 минут
//Расстояние для Voyager 2 увеличится на +1 million km примерно через 18 часов 31 минуту

static struct proc_dir_entry *proc_file;

static ssize_t procfile_read(struct file *file, char __user *buffer,
                             size_t len, loff_t *offset)
{
    char msg[512];
    time64_t now = ktime_get_real_seconds();
    s64 elapsed_sec = now - BASE_TIMESTAMP;

    unsigned long months, days, hours, minutes, secs;
    unsigned long v1_km, v2_km, diff_km;
    unsigned long delta_v1_km, delta_v2_km;

    if (*offset > 0)
        return 0;

    if (elapsed_sec < 0)
        elapsed_sec = 0;

    v1_km = BASE_V1_KM + SPEED_V1 * elapsed_sec;
    v2_km = BASE_V2_KM + SPEED_V2 * elapsed_sec;
    diff_km = v1_km - v2_km;

    delta_v1_km = SPEED_V1 * elapsed_sec;
    delta_v2_km = SPEED_V2 * elapsed_sec;

    months  = elapsed_sec / (30 * 24 * 3600);
    days    = (elapsed_sec % (30 * 24 * 3600)) / (24 * 3600);
    hours   = (elapsed_sec % (24 * 3600)) / 3600;
    minutes = (elapsed_sec % 3600) / 60;
    secs    = elapsed_sec % 60;

    int msg_len = snprintf(msg, sizeof(msg),
        "Voyager 1 distance from Earth: %lu million km\n"
        "Voyager 2 distance from Earth: %lu million km\n"
        "Voyager 1 is farther by: %lu million km\n\n"
        "Elapsed since 2025-12-18:\n"
        "%lu months %lu days %lu hours %lu minutes %lu seconds\n\n"
        "Distance increase since base date:\n"
        "Voyager 1: +%lu million km\n"
        "Voyager 2: +%lu million km\n",
        v1_km / 1000000,
        v2_km / 1000000,
        diff_km / 1000000,
        months, days, hours, minutes, secs,
        delta_v1_km / 1000000,
        delta_v2_km / 1000000);

    if (copy_to_user(buffer, msg, msg_len))
        return -EFAULT;

    *offset = msg_len;
    return msg_len;
}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0)
static const struct proc_ops proc_fops = {
    .proc_read = procfile_read,
};
#else
static const struct file_operations proc_fops = {
    .read = procfile_read,
};
#endif

static int __init tsu_module_init(void)
{
    proc_file = proc_create(PROC_NAME, 0444, NULL, &proc_fops);
    if (!proc_file)
        return -ENOMEM;

    pr_info("Welcome to the Tomsk State University\n");
    return 0;
}

static void __exit tsu_module_exit(void)
{
    proc_remove(proc_file);
    pr_info("Tomsk State University forever!\n");
}

module_init(tsu_module_init);
module_exit(tsu_module_exit);
MODULE_LICENSE("GPL");
