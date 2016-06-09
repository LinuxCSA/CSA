# CSA
Comprehensive System Accounting for Linux

### What is Comprehensive System Accounting?
CSA is a more extensive alternative to existing Linux process accounting packages. It exists as a set of C programs and shell scripts that provide methods for collecting per-process resource usage data, monitoring disk usage, and charging fees to specific login accounts. CSA takes this per-process accounting information and combines it by job identifier (jid) for tracking groups of related processes within system boot uptime periods.

CSA provides the following features which are not available with other Linux accounting packages:

- Per-job accounting
- Daemon accounting
- User job accounting (`ja` command)
- Flexible accounting periods (not just daily and monthly periods)
- Flexible system billing units (SBUs)
- Offline archiving of accounting data
- User exits for site specific customization of reports
- Configurable parameters

How else is this different from other Linux accounting packages? Linux has a form of process accounting which is similar to SysV/BSD basic process accounting. The set of Linux accounting commands provide similar functionality but have different names and options.

While the Linux kernel itself monitors the size of the accounting records file (pacct), CSA monitoring of file size takes place outside of the kernel.

Linux CSA is the outcome of a joint effort between Los Alamos National Laboratory (LANL) and SGI to port the IRIX CSA tool to the Linux Operating System. This project is a community driven fork of that code base and is not supported by either organization.

### More Info
For additional information and instructions on proper usage, please visit the  [Wiki](https://github.com/LinuxCSA/CSA/wiki).
