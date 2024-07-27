# Oracle Home Configuration Tool

This Windows console application is designed to streamline the process of managing Oracle Home network configuration files. The tool copies `tnsnames.ora` and `sqlnet.ora` files to all Oracle Home directories, with specific handling for additional configuration files if they are present.

## Features

- **Copy `tnsnames.ora` and `sqlnet.ora` to all Oracle Home directories**
- **Append `tnsnames-config.ora` to `tnsnames.ora` if found**
- **Replace `sqlnet.ora` with `sqlnet-config.ora` if found**

## Usage

Run the application from the command line to automatically manage the Oracle Home configuration files.

```shell
tns.exe
```

## How It Works

1. The tool locates all Oracle Home directories on the system.
2. It copies the `tnsnames.ora` and `sqlnet.ora` files to the `{ORACLE_HOME}\NETWORK\ADMIN` directory.
3. If a `tnsnames-config.ora` file is found in `{ORACLE_HOME}\NETWORK\ADMIN`, its contents are appended to the `tnsnames.ora` file.
4. If a `sqlnet-config.ora` file is found in `{ORACLE_HOME}\NETWORK\ADMIN`, it replaces the `sqlnet.ora` file.

## Author
- [Goran Katavić](https://github.com/elc64)

## Acknowledgments

- Oracle Corporation for their database technology.