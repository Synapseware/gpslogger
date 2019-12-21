AT25DF321
=========

Driver library for interfacing with an Atmel AT25DF321 device.  Supports stream-like functions.


# Usage

## Reading data

### Read 1-n bytes from the device

- flash.open()
- flash.seekg(address)
- flash.get()
- flash.close()

### Block read

- flash.open()
- flash.seekg(address)
- flash.read(char*, int)
- flash.close()


## Writing data

### Write 1-n bytes to the device

- flash.open()
- flash.seekp(address)
- flash.put(char)
- flash.close()


### Block write

- flash.open()
- flash.seekp(address)
- flash.write(const char*, int count)
- flash.close()


## Erasing
