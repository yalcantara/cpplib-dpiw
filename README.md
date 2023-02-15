# C++ | cpplib-dpiw
A thin wrapper for ODPI, that simplify most commons task such as, preparing statements and executing queries.

## Features
- Easy to use
- Header-only
- Stack-based objects
- Auto handling of resources

## Development
- C++ 17
- Docker
- clang
- cmake

## To compile & run 🔥
Since the cpplib-dpiw depends on ODPI, make sure to clone the repo (https://github.com/oracle/odpi) at the same 
directory level where cpplib-dpiw is. Also, clone cpplib-core in the same directory level.

First, build the Docker:
```shell
./build-image.sh
```
Then, compile:
```shell
./compile.sh
```

Also make sure the `LD_LIBRARY_PATH`, `DYLD_LIBRARY_PATH` in macOS, environment 
variable, contains the path to Oracle instant client binary folder.

```shell
// Linux
LD_LIBRARY_PATH=/opt/oracle/instantclient_19_8

// macOS
DYLD_LIBRARY_PATH=/opt/oracle/instantclient_19_8
```

### Running locally
To run the `main` program, besides doing the above steps, you'll also need to set the following environment variables:

```shell
app_user = <username>
app_pass = <password>
app_tnsp = <TNS connection>
```
If you'll like to use Oracle cloud wallet, you can specify the path where the wallet is in the TNS .ora file. Please 
consult Oracle Documentation regarding connection strings, TNS, and Client Wallet.

Developers note: In your IDE of choice you can pass the above environment variable by configuring the 
Run/Debug configuration. 

## Usage
Start by cloning the repo in a path that is easy to search from your project. Add to your include folder 
cpplib-dpiw/include. Link with **oci**.

### Example
```cpp
DBEnvironment env;

DBConnection conn = env.connect(user, pass, tnsp);
DBStatement stm = conn.statement("SELECT table_name, num_rows FROM user_tables");

println("table_name     num_rows");
stm.execQuery().forEach([](ResultSet &r){
    printf("%s  %d\n", r.getString(1).c_str(), r.getInt32(2));
});
```

## Contributing
Contributions are welcome to the cpplib-dpiw library! If you have a bugfix or new feature, please create a pull 
request. If you have any questions, feel free to open an issue.