# Install for ESP Component Manager

This is the documentation for the wolfSSL install / publish to [components.espressif.com](https://components.espressif.com/components/wolfssl/wolfssl).

See the [Espressif idf-component-manager docs](https://docs.espressif.com/projects/idf-component-manager/en/latest/).

Configuration for the component is in the top-level [idf_component.yml](./idf_component.yml) file.
Note that this is *different* from the same-name files in the example projects.

See the `wolfssl_component_publish.sh` bash script. Set private `IDF_COMPONENT_API_TOKEN`
environment variable as appropriate. Optionally set the `IDF_COMPONENT_REGISTRY_URL`.
Typically there's only one valid option. See [Staging](./INSTALL.md#Staging), below.

The script automates the process of populating a directory with wolfSSL source code and examples to publish.
The core command for this is:

```bash
compote component upload --namespace wolfssl --name wolfssl
```

The output can be found in the `dist` directory, for example a file called `wolfssl_5.6.0-stable.tgz` and
directory of the contents:

```text
wolfssl_5.6.0-stable
wolfssl_5.6.0-stable.tgz
```

Examples are copied into the local [./examples/](./examples/README.md) directory.

Each example project needs to have a `main/idf_component.yml` file,
as well as a file called `init_demo.sh`.

The example project `idf_component.yml` file should be edited as needed. Typical contents:

```
## IDF Component Manager Manifest File
dependencies:
  wolfssl/wolfssl: "^5.6.0-stable"
  ## Required IDF version
  idf:
    version: ">=4.1.0"
```

## Staging

There's a staging site at https://components-staging.espressif.com/ for testing deployments.

To use this, set the `IDF_COMPONENT_REGISTRY_URL` environment variable:

```
export IDF_COMPONENT_REGISTRY_URL=https://components-staging.espressif.com/ 
```

The default when not set is the production site at https://components.espressif.com

## License File

License field is added to the manifest: See [docs](https://docs.espressif.com/projects/idf-component-manager/en/latest/reference/manifest_file.html#manifest-file-idf-component-yml-format-reference).
The [spdx format license text](https://spdx.org/licenses/) is used.

## ESP Component Examples

Note that when the ESP Component manager installs wolfSSL, then the source code for wolfSSL
will be *in the local component directory*. Normally there's only a cmake file that points
to where the wolfSSL library is located.

Managed components are distinguished by the `idf_component.yml` file in the `projectname/main` directory.

The wolfSSL component must be either managed or non-managed. Not both.

## Non-ESP Component Example

For a wolfSSL getting started example, see the basic [wolfSSL Template Project](https://github.com/wolfSSL/wolfssl/tree/master/IDE/Espressif/ESP-IDF/examples/template)
and the [other examples](https://github.com/wolfSSL/wolfssl/tree/master/IDE/Espressif/ESP-IDF/examples).

Non-managed components are distinguished by having no `idf_component.yml` file in the `projectname/main` directory.
Instead there's a `components/wolfssl/CMakeLists.txt` file.

The wolfSSL component must be either managed or non-managed. Not both.

## Test build of examples

Set `WOLFSSL_ROOT` to the location of the to-be-published wolfSSL directory:

```
export WOLFSSL_ROOT=/mnt/c/workspace/wolfssl-gojimmypi-PR/IDE/Espressif/component-manager/
```

The component files in [./lib/components/wolfssl](./lib/components/wolfssl/README.md) will
be copied to each respective example project to confirm they will build.

There's a [wolfssl_build_example.sh](./wolfssl_build_example.sh) script the will temporarily remove
the project `./main/idf_component.yml` component file to ensure the currently-published ESP Registry code
is not used during the build test. That script is called for each of the [component manager examples](./examples/README.md).

The source code for the local examples to be published is copied from [Espressif/ESP-IDF/examples](../ESP-IDF/examples/README.md).


## Common Problems


When there's both `idf_component.yml` file in the `projectname/main` and a
`components/wolfssl/CMakeLists.txt` file, an error such as this will occur:

```
CMake Error at /mnt/c/SysGCC/esp32/esp-idf/v5.1/tools/cmake/component.cmake:250 (message):
  ERROR: Cannot process component requirements.  Multiple candidates to
  satisfy project requirements:

    requirement: "wolfssl" candidates: "wolfssl, wolfssl__wolfssl"
```

To resolve, either:

* Remove the `idf_component.yml` file and remove wolfssl directory from `projectname/managed__components`
* Remove the wolfssl directory from `projectname/components`

