/*
 * See documentation at https://nRF24.github.io/RF24
 * See License information at root directory of this library
 * Author: Brendan Doherty 2bndy5
 */

/**
 * A simple example of streaming data from 1 nRF24L01 transceiver to another.
 *
 * This example was written to be used on 2 devices acting as "nodes".
 * Use the Serial Terminal to change each node's behavior.
 */
#include "pico/stdlib.h" // printf(), sleep_ms(), getchar_timeout_us(), to_us_since_boot(), get_absolute_time()
#include <tusb.h>        // tud_cdc_connected()
#include "math.h"        // abs()
#include "../RF24.h"     // RF24 radio object

// instantiate an object for the nRF24L01 transceiver
RF24 radio(7, 8); // using pin 7 for the CE pin, and pin 8 for the CSN pin

// Used to control whether this node is sending or receiving
bool role = false; // true = TX node, false = RX node

// For this example, we'll be sending 32 payloads each containing
// 32 bytes of data that looks like ASCII art when printed to the serial
// monitor. The TX node and RX node needs only a single 32 byte buffer.
#define SIZE 32            // this is the maximum for this example. (minimum is 1)
char buffer[SIZE + 1];     // for the RX node
uint8_t counter = 0;       // for counting the number of received payloads
void makePayload(uint8_t); // prototype to construct a payload dynamically


bool setup()
{
    buffer[SIZE] = 0; // add a NULL terminating character (for easy printing)

    // Let these addresses be used for the pair
    uint8_t address[][6] = {"1Node", "2Node"};
    // It is very helpful to think of an address as a path instead of as
    // an identifying device destination

    // to use different addresses on a pair of radios, we need a variable to
    // uniquely identify which address this radio will use to transmit
    bool radioNumber; // 0 uses address[0] to transmit, 1 uses address[1] to transmit

    // wait here until the CDC ACM (serial port emulation) is connected
    while (!tud_cdc_connected()) {
        sleep_ms(10);
    }

    // initialize the transceiver on the SPI bus
    if (!radio.begin()) {
        printf("radio hardware is not responding!!\n");
        return false;
    }

    // print example's introductory prompt
    printf("RF24/examples_pico/streamingData");

    // To set the radioNumber via the Serial monitor on startup
    printf("Which radio is this? Enter '0' or '1'. Defaults to '0' ");
    char input = getchar();
    radioNumber = input == 49;
    printf("radioNumber = %d\n", (int)radioNumber);

    // Set the PA Level low to try preventing power supply related problems
    // because these examples are likely run with nodes in close proximity to
    // each other.
    radio.setPALevel(RF24_PA_LOW);  // RF24_PA_MAX is default.

    // save on transmission time by setting the radio to only transmit the
    // number of bytes we need to transmit
    radio.setPayloadSize(SIZE);     // default value is the maximum 32 bytes

    // set the TX address of the RX node into the TX pipe
    radio.openWritingPipe(address[radioNumber]);     // always uses pipe 0

    // set the RX address of the TX node into a RX pipe
    radio.openReadingPipe(1, address[!radioNumber]); // using pipe 1

    // additional setup specific to the node's role
    if (role) {
        radio.stopListening();  // put radio in TX mode
    }
    else {
        radio.startListening(); // put radio in RX mode
    }

    // For debugging info
    // radio.printDetails();       // (smaller) function that prints raw register values
    radio.printPrettyDetails(); // (larger) function that prints human readable data

    // role variable is hardcoded to RX behavior, inform the user of this
    printf("*** PRESS 'T' to begin transmitting to the other node\n");

    return true;
} // setup()


void loop() {

    if (role) {
        // This device is a TX node

        radio.flush_tx();
        uint8_t i = 0;
        uint8_t failures = 0;
        uint64_t start_timer = to_us_since_boot(get_absolute_time()); // start the timer
        while (i < SIZE) {
            makePayload(i);                                           // make the payload
            if (!radio.writeFast(&buffer, SIZE)) {
                failures++;
                radio.reUseTX();
            }
            else {
                i++;
            }

            if (failures >= 100) {
                printf("Too many failures detected. Aborting at payload %c", buffer[0]);
                break;
            }
        }
        uint64_t end_timer = to_us_since_boot(get_absolute_time()); // end the timer

        // print results from transmitting stream
        printf("Time to transmit = %d us with %d failures detected", end_timer - start_timer, failures);

        // to make this example readable in the serial terminal
        sleep_ms(500); // slow transmissions down by 0.5 second (+ another 0.5 second for user input later)
    }
    else {
        // This device is a RX node

        if (radio.available()) {         // is there a payload?
            radio.read(&buffer, SIZE);     // fetch payload from FIFO

            // print the received payload and its counter
            printf("Received: %s - #d", buffer, counter++);
        }
    } // role

    char input = getchar_timeout_us(500); // wait 0.5 second for user input
    if (input != PICO_ERROR_TIMEOUT) {
        // change the role via the serial terminal

        if ((input == 'T' || input == 't') && !role) {
            // Become the TX node

            role = true;
            counter = 0; //reset the RX node's counter
            printf("*** CHANGING TO TRANSMIT ROLE -- PRESS 'R' TO SWITCH BACK\n");
            radio.stopListening();

        }
        else if ((input == 'R' || input == 'r') && role) {
            // Become the RX node

            role = false;
            printf("*** CHANGING TO RECEIVE ROLE -- PRESS 'T' TO SWITCH BACK\n");
            radio.startListening();
        }
    }
} // loop


void makePayload(uint8_t i) {
    // Make a single payload based on position in stream.
    // This example employs function to save memory on certain boards.

    // let the first character be an identifying alphanumeric prefix
    // this lets us see which payload didn't get received
    buffer[0] = i + (i < 26 ? 65 : 71);
    for (uint8_t j = 0; j < SIZE - 1; ++j) {
        char chr = j >= (SIZE - 1) / 2 + abs((SIZE - 1) / 2 - i);
        chr |= j < (SIZE - 1) / 2 - abs((SIZE - 1) / 2 - i);
        buffer[j + 1] = chr + 48;
    }
}

int main()
{
    stdio_init_all(); // init necessary IO for the RP2040

    bool setup_ok = setup();
    while (setup_ok) {
        loop();
    }
    return (int)setup_ok;
}
