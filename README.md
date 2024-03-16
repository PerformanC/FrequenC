# FrequenC

The standalone audio-sending node written in C.

## Features

- C99 compliant
- Standalone
- Low resource usage

> [!NOTE]
> FrequenC is not LavaLink compatible. Use [NodeLink](https://github.com/PerformanC/NodeLink).

## Dependencies

- [`OpenSSL`](https://www.openssl.org/)

> [!NOTE]
> The PerformanC Organization recommends the use of the latest version of OpenSSL.

## Installation

### 1. Clone the repository

```shell
$ git clone https://github.com/PerformanC/FrequenC
```

### 2. Compile the project

```shell
$ make -j4
```

> [!NOTE]
> The `-j4` flag is used to compile the project using 4 threads. You can change the number to the number of threads you want to use.

> [!NOTE]
> If you want to compile it on debug mode, add the `CFLAGS="-g"` flag to the `make` command.

### 3. Run the project

```shell
$ ./FrequenC
```

## Usage

We don't have a client for FrequenC yet. PerformanC is working on porting [FastLink](https://github.com/PerformanC/FastLink) to FrequenC API. Stay tuned for updates.

## Documentation

The [FrequenC API is documented in API.md file](docs/API.md). It contains all the information about the API, and how to use it.

## Support

Any question or issue related to FrequenC or other PerformanC projects can be made in [PerformanC's Discord server](https://discord.gg/uPveNfTuCJ).

## Contribution

It is mandatory to follow the PerformanC's [contribution guidelines](https://github.com/PerformanC/contributing) to contribute to FrequenC. Following its Security Policy, Code of Conduct and syntax standard.

## Projects using FrequenC

None yet, but we are looking forward to see your project here.

## License

FrequenC is licensed under [BSD 2-Clause License](LICENSE). You can read more about it on [Open Source Initiative](https://opensource.org/licenses/BSD-2-Clause).

* This project is considered as: [leading standard](https://github.com/PerformanC/contributing?tab=readme-ov-file#project-information).
