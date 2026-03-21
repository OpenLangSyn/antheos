# Antheos Protocol

**Version 1.0 — Level 1 Specification**

© Are Bjørby
February 2026
*LangSyn Project*

---

# 1. Introduction

Antheos (Ant) is an anonymous, lightweight, decentralized communication protocol with a wrap-and-tunnel philosophy and a bus abstraction that utilizes existing hardware and software. It is designed to operate on any transport — from a UART serial line to a TCP socket, from a LoRa radio to a human carrying a USB drive.

This document defines the Level 1 specification: the complete, self-contained procedural protocol covering transport (bus operations), service discovery, and session management. Level 1 is the universal baseline. Every verb does exactly one thing. It runs on all devices from MCU to server cluster.

Antheos is designed to support capability tiers beyond Level 1. Higher levels are defined in separate specifications and are fully optional. A Level 1 implementation is complete and conformant without knowledge of any higher level.

---

# 2. Design Principles

- **Simplicity**: Only devices and buses exist. No routers, no controllers, no central authority.
- **Decentralization**: All instances are peers. Identity is established through registries; communication is peer-to-peer.
- **Universality**: CP437 encoding ensures every message is displayable on any terminal, printer, or display manufactured since 1981.
- **Scalability**: The same protocol frames flow from server clusters to 8-bit microcontrollers. Constrained devices handle what they can and drop what they cannot.
- **Wrap and Tunnel**: Antheos wraps its messages and tunnels through any transport that can carry bytes. The bus abstraction is agnostic to the physical or virtual medium.
- **Query, Don't Advertise**: Instances discover capabilities by querying, not by advertising. No upfront capability handshakes. No schema exchanges.

---

# 3. Terms and Definitions

| **Term**   | **Definition**                                                     |
| :--------- | :----------------------------------------------------------------- |
| Text       | CP437 characters excluding reserved control characters             |
| Blob       | Binary data (arbitrary bytes)                                      |
| Instance   | Device, software, or HID endpoint communicating on a network       |
| Network    | Instances connected to one or more buses                           |
| Bus        | Device, software, system, or person facilitating instance communication |
| Message    | A head of words followed by an optional binary tail                |
| Head       | The CP437 portion of a message, containing words                   |
| Word       | A typed, delimited unit within the head                            |
| Tail       | Optional binary data following the head                            |
| Flag       | A qualifier within a word (radix or unit)                          |
| Body       | The content portion of a word                                      |
| Session    | A separate group of related messages between instances             |
| Service    | A session offered by an instance to accomplish a specific task     |
| Origin     | Source, creator, or owner of instances                             |
| Id         | A text or number used to identify objects                          |
| Registry   | An entity maintaining a list of identifiers                        |

---

# 4. Protocol Encoding

Antheos uses CP437 (Code Page 437), the character encoding developed by IBM for the original PC in 1981. CP437 is a single-byte encoding defining 256 characters. The first 128 (0x00–0x7F) align with ASCII. The upper 128 (0x80–0xFF) include accented letters, Greek symbols, and box-drawing characters.

Antheos leverages CP437's universal hardware and software support. Every message is human-readable on any device that can display CP437 — which includes virtually all computing hardware ever manufactured. The protocol reserves 7 bytes from the CP437 control character range (0x00–0x1F) as structural delimiters.

## 4.1 Reserved Control Characters

All reserved bytes were selected to avoid collisions with ASCII flow control (XON 0x11, XOFF 0x13), terminal escapes (ESC 0x1B), whitespace (TAB 0x09, LF 0x0A, CR 0x0D), and null terminators (0x00). This ensures Antheos frames pass transparently through serial links with software flow control, terminal emulators, and C string handling.

| **Hex** | **Abbr** | **CP437 Glyph** | **Role**                 |
| :------ | :------- | :-------------- | :----------------------- |
| 0x02    | SOM      | ☻               | Start of Message         |
| 0x03    | EOM      | ♥               | End of Message           |
| 0x04    | SOR      | ♦               | Start of Radix qualifier |
| 0x07    | SOU      | •               | Start of Unit qualifier  |
| 0x10    | EOW      | ►               | End of Word              |
| 0x12    | SOW      | ↕               | Start of Word            |
| 0x1A    | SOB      | →               | Start of Body            |

These 7 bytes are forbidden in word bodies and text content. Their presence in the byte stream always indicates protocol structure, enabling stream parsing without buffering entire messages.

---

# 5. Identifier Types

Antheos uses a layered identity model. Each identifier type has a defined scope, format, and lifetime. The layers build from organizational identity (OID) down to per-message sequencing (MID).

| **Id** | **Name**  | **Scope**  | **Format** | **Lifetime**                              |
| :----- | :-------- | :--------- | :--------- | :---------------------------------------- |
| OID    | Origin    | Registry   | ASCII      | Registry-defined                          |
| DID    | Device    | Origin     | ASCII      | Origin-defined (hardwired type identifier)|
| IID    | Instance  | Origin     | ASCII      | Origin-defined (hardwired serial)         |
| BID    | Bus       | Bus        | Base-32    | Ephemeral (established at bus connection) |
| SID    | Session   | Instance   | Base-32    | Session duration (fresh per session)      |
| MID    | Message   | Session    | Decimal    | Message duration (sequential counter)     |

## 5.1 OID (Origin Identifier)

The OID identifies the origin — the organization, individual, or entity that created and owns instances. OIDs are maintained by registries. An OID is a plaintext ASCII string. Carried in an ID word without radix flag.

## 5.2 DID (Device Identifier)

The DID identifies the device type within an origin's namespace. It is a hardwired, human-readable ASCII string (e.g., "Thermostat", "SensorV2"). Defined by the origin, not the network. Carried in an ID word without radix flag.

## 5.3 IID (Instance Identifier)

The IID identifies a specific instance of a device type. It is a hardwired unique serial (e.g., "SN00482"). Together, OID:DID:IID uniquely identifies any physical or virtual instance in the world. Carried in an ID word without radix flag.

## 5.4 BID (Bus Identifier)

The BID is a base-32 address negotiated at bus connection through the Establish/Conflict protocol (see Section 9.1). BIDs are ephemeral and local to a single bus. They are kept as short as possible, growing only on collision. A device connected to multiple buses holds a separate BID on each. Carried in an ID word with radix flag U (duotrigesimal).

## 5.5 SID (Session Identifier)

The SID identifies a session between instances. It is a base-32 hash of OID:DID:IID:\<counter\>. Every new session receives a fresh SID — SIDs are never reused. A monotonic counter ensures each SID is unique; the counter resets on device reboot. Carried in an ID word with radix flag U (duotrigesimal).

## 5.6 MID (Message Identifier)

The MID is a decimal counter that increments sequentially within a session, starting at 1. MID=0 is the wrap signal (see Section 11.1.2). Carried in an ID word with radix flag D (decimal).

## 5.7 Identifier Encoding Summary

All identifiers are carried in ID words (@). The ID word optionally includes a radix flag to specify the encoding of numeric identifiers. Unit flags are never used with ID words.

| **Identifier** | **Radix**    | **Wire Example**  |
| :------------- | :----------- | :---------------- |
| OID            | (none)       | ↕@→langsyn►       |
| DID            | (none)       | ↕@→Thermostat►    |
| IID            | (none)       | ↕@→SN00482►       |
| BID            | U (base-32)  | ↕@♦U→4T9X2►       |
| SID            | U (base-32)  | ↕@♦U→A7K2M►       |
| MID            | D (decimal)  | ↕@♦D→1►           |

**BID/SID Disambiguation:** Both BID and SID use base-32 encoding with radix flag U. The verb scope determines which identifier type is expected:

- **Bus-scope verbs** (E, C, B, P, R, D, V, S, W, X): base-32 IDs are BIDs
- **Service-scope verbs** (Q, O, A): base-32 IDs are BIDs (addressing the offering instance)
- **Session-scope verbs** (K, T, N, L, U, F): the first base-32 ID is SID; a second base-32 ID, if present, is a BID

A receiver determines identifier type by the verb in the SYMBOL word, not by inspecting the identifier value itself.

---

# 6. Word Encoding

A word is the fundamental data unit within an Antheos message head. Each word is delimited by SOW (0x12) and EOW (0x10), with typed content between them.

## 6.1 Word Structure

The general word structure is:

```
[SOW] [WT] ([SOR][RF]) ([SOU][UF]) [SOB] Body [EOW]
```

**WT** (Word Type) is always the first byte after SOW. **SOR/RF** (Radix Flag) and **SOU/UF** (Unit Flag) are conditionally present depending on the word type. **SOB** marks the start of the body content. **EOW** terminates the word.

Flag requirements vary by word type:

| **Category**                   | **Radix (SOR/RF)** | **Unit (SOU/UF)** | **Word Types**                                   |
| :----------------------------- | :----------------- | :---------------- | :----------------------------------------------- |
| Radix + Unit required          | Required           | Required          | INTEGER (#), REAL ($), SCIENTIFIC (%), BLOB (\*) |
| Radix optional, Unit forbidden | Optional           | Forbidden         | ID (@)                                           |
| Both forbidden                 | Forbidden          | Forbidden         | SYMBOL (!), PATH (/), TEXT ("), LOGICAL (?), TIMESTAMP (&), MESSAGE (~) |

## 6.2 Word Types

| **CP437** | **Hex** | **Type**    | **Description**                                     | **Flags**       |
| :-------- | :------ | :---------- | :-------------------------------------------------- | :-------------- |
| !         | 0x21    | SYMBOL      | Protocol verbs and markers                          | None            |
| @         | 0x40    | ID          | Identifiers (OID, DID, IID, BID, SID, MID)          | Radix optional  |
| /         | 0x2F    | PATH        | Routing sequences                                   | None            |
| "         | 0x22    | TEXT        | Plain text content                                  | None            |
| #         | 0x23    | INTEGER     | Integer values                                      | Radix + Unit    |
| $         | 0x24    | REAL        | Floating-point values                               | Radix + Unit    |
| %         | 0x25    | SCIENTIFIC  | Exponential notation                                | Radix + Unit    |
| ?         | 0x3F    | LOGICAL     | Logical/boolean expressions                         | None            |
| &         | 0x26    | TIMESTAMP   | ISO 8601 date/time values                           | None            |
| \*        | 0x2A    | BLOB        | Tail size declaration (unit = size field width)     | Radix + Unit    |
| ~         | 0x7E    | MESSAGE     | Embedded message reference                          | None            |

## 6.3 Radix Flags

Specifies the base encoding of a numeric body. Follows the SOR (0x04) delimiter.

| **Flag** | **Name**        | **Base** |
| :------- | :-------------- | :------- |
| I        | Binary          | Base 2   |
| O        | Octal           | Base 8   |
| D        | Decimal         | Base 10  |
| H        | Hexadecimal     | Base 16  |
| U        | Duotrigesimal   | Base 32  |

## 6.4 Unit Flags

Specifies the data width of a numeric value. Follows the SOU (0x07) delimiter. Required for INTEGER, REAL, SCIENTIFIC, and BLOB word types. Forbidden for all other types including ID.

| **Flag** | **Name**   | **Size**   |
| :------- | :--------- | :--------- |
| B        | Byte       | 8 bits     |
| W        | Word       | 16 bits    |
| D        | Doubleword | 32 bits    |
| Q        | Quadword   | 64 bits    |
| M        | Megabyte   | 2²⁰ bytes  |
| G        | Gigabyte   | 2³⁰ bytes  |
| T        | Terabyte   | 2⁴⁰ bytes  |

## 6.5 Word Encoding Examples

Each example shows the pseudo form (using abbreviations) and the wire form (using CP437 glyphs). In wire form: ↕=SOW, ♦=SOR, •=SOU, →=SOB, ►=EOW.

**SYMBOL word** (verb "Establish"):

```
Pseudo:  [SOW] ! [SOB] E [EOW]
Wire:    ↕!→E►
```
*No flags. Body is a single character.*

**ID word** (ASCII, no radix — OID):

```
Pseudo:  [SOW] @ [SOB] langsyn [EOW]
Wire:    ↕@→langsyn►
```
*Plain ASCII identifier. No radix or unit flags.*

**ID word** (base-32 radix — BID):

```
Pseudo:  [SOW] @ [SOR]U [SOB] 4T9X2 [EOW]
Wire:    ↕@♦U→4T9X2►
```
*Radix flag U (duotrigesimal). No unit flag.*

**ID word** (decimal radix — MID):

```
Pseudo:  [SOW] @ [SOR]D [SOB] 1 [EOW]
Wire:    ↕@♦D→1►
```
*Radix flag D (decimal). No unit flag.*

**TEXT word**:

```
Pseudo:  [SOW] " [SOB] temperature [EOW]
Wire:    ↕"→temperature►
```
*No flags. Body is plain text.*

**PATH word** (dot-separated BID sequence):

```
Pseudo:  [SOW] / [SOB] AA.BB.CC.DD [EOW]
Wire:    ↕/→AA.BB.CC.DD►
```
*No flags. Body is a dot-separated sequence of BIDs forming a route.*

**INTEGER word** (decimal, byte-width, value 32):

```
Pseudo:  [SOW] # [SOR]D [SOU]B [SOB] 32 [EOW]
Wire:    ↕#♦D•B→32►
```
*Radix D (decimal), Unit B (byte). Both required.*

**INTEGER word** (hex, doubleword, value FF00):

```
Pseudo:  [SOW] # [SOR]H [SOU]D [SOB] FF00 [EOW]
Wire:    ↕#♦H•D→FF00►
```
*Radix H (hex), Unit D (doubleword).*

**LOGICAL word** (exclusion expression):

```
Pseudo:  [SOW] ? [SOB] !H&!Q [EOW]
Wire:    ↕?→!H&!Q►
```
*No flags. Body is a boolean expression: NOT hex AND NOT quadword.*

**BLOB word** (hex, doubleword, declaring 6656 byte tail block):

```
Pseudo:  [SOW] * [SOR]H [SOU]D [SOB] 1A00 [EOW]
Wire:    ↕*♦H•D→1A00►
```
*Radix H, Unit D. Declares a tail block of 0x1A00 (6656) bytes.*

**TIMESTAMP word**:

```
Pseudo:  [SOW] & [SOB] 2026-02-08T12:00:00Z [EOW]
Wire:    ↕&→2026-02-08T12:00:00Z►
```
*No flags. Body is ISO 8601.*

---

# 7. Message Frame Structure

## 7.1 Frame Layout

```
[SOM] Word₁ Word₂ ... Wordₙ [EOM] Tail₁ Tail₂ ...
```

A message begins with SOM (0x02) and the head ends with EOM (0x03). The head contains one or more words, each delimited by SOW/EOW. The optional tail follows EOM and consists of binary data blocks.

## 7.2 Head

The head is the structured, CP437-encoded portion of the message. It contains words that specify the operation, addressing, parameters, and any tail size declarations. Words are parsed sequentially as they arrive — a receiver does not need to buffer the entire head before processing.

## 7.3 Tail

The tail is optional binary data following EOM. It is partitioned into blocks whose sizes are declared by BLOB (\*) words in the head, indexed by their position. The first BLOB word declares the size of the first tail block, the second BLOB word the second block, and so on. A receiver that has parsed the head knows the exact byte count of the tail before the first tail byte arrives.

**BLOB Word Semantics:** The BLOB body is a number representing the byte count of the corresponding tail block. The radix flag specifies the number's encoding (decimal, hex, etc.) and the unit flag specifies the numeric range (B = 8-bit, W = 16-bit, D = 32-bit, Q = 64-bit). For example, `*♦D•B→10` declares a 10-byte block; `*♦H•D→1A00` declares a 6656-byte block.

**Example**: A message with two BLOB words in the head:

```
[SOM] ...words... [SOW]*[SOR]H[SOU]D[SOB]0100[EOW] [SOW]*[SOR]H[SOU]D[SOB]0200[EOW] [EOM] <256 bytes> <512 bytes>
```

Both BLOB words use unit D (doubleword / 32-bit size field) and radix H (hexadecimal). The first declares 0x0100 = 256 bytes. The second declares 0x0200 = 512 bytes. Total tail length is 768 bytes.

BLOB words are positional and non-repeating within a single message head. Each BLOB word declares exactly one tail block, indexed by its ordinal position in the head. A message head containing more BLOB words than tail blocks, or a tail shorter than the sum of all declared BLOB sizes, is malformed. A receiver that detects either condition MUST discard the message and respond with `!X MALFORMED_FRAME`. A receiver that has already begun consuming tail bytes when a size mismatch is detected MUST scan forward to the next SOM byte and report `!X MALFORMED_FRAME`.

## 7.4 Stream Parsing

Antheos is designed for stream parsing. A receiver processes one byte at a time:

1. Wait for SOM (0x02). All bytes before SOM are discarded.
2. Parse words as SOW/EOW pairs arrive. Each word is self-describing: word type, optional radix flag, optional unit flag, and body content.
3. On EOM (0x03), the head is complete. Sum the sizes declared by any BLOB words to determine the total tail length.
4. Read tail bytes until the declared total is reached. The next SOM starts a new message.

If at any point the receiver cannot continue (buffer full, unsupported type, malformed word), it either sends a Scaleback response or silently drops the frame and scans forward for the next SOM byte.

## 7.5 Messages Without Tail

A message with no BLOB words in the head has no tail. The byte immediately following EOM is either the SOM of the next message or non-message bus traffic. This is the common case for control messages (Establish, Conflict, Ping, etc.).

## 7.6 Word Order

Words within a message head MUST appear in the following order:

1. **SYMBOL word** — exactly one, always the first word.
2. **ID words** — zero or more, in order: BID (if present), SID (if present), MID (if present).
3. **Payload words** — zero or more, in application-defined order.

---

# 8. Level 1 Symbols

Level 1 defines three scopes of operation: bus, service, and session. Symbols are single ASCII characters following the SYMBOL word type (!). Every symbol is unique across all scopes. No disambiguation is required.

## 8.1 Bus Scope

Bus scope operations manage transport-level concerns: addressing, message relay, keepalive, capability negotiation, and error reporting.

| **Symbol** | **Verb**    | **Description**                            |
| :--------- | :---------- | :----------------------------------------- |
| E          | Establish   | Claim a BID on this bus                    |
| C          | Conflict    | Report a BID collision                     |
| B          | Broadcast   | Send to all instances on this bus          |
| P          | Ping        | Keepalive / presence check                 |
| R          | Relay       | Forward a message to another bus           |
| D          | Discover    | Find which bus a BID resides on            |
| V          | Verify      | Resolve BID to full OID:DID:IID identity   |
| S          | Scaleback   | Declare receiver capability limits (reactive) |
| W          | Acknowledge | Confirm receipt of a bus-scope message     |
| X          | Exception   | Report a structured error                  |

## 8.2 Service Scope

Service scope operations handle runtime capability discovery. An instance that wants a capability queries for it. Instances that can provide it offer. The querying instance accepts. No upfront advertisement, no schema exchange.

| **Symbol** | **Verb** | **Description**                                     |
| :--------- | :------- | :-------------------------------------------------- |
| Q          | Query    | Request a service or capability                     |
| O          | Offer    | Offer to provide the requested service              |
| A          | Accept   | Accept an offer, establishing the service agreement |

## 8.3 Session Scope

Session scope operations manage active communication sessions between instances. Sessions are optional — not every interaction requires one.

| **Symbol** | **Verb** | **Description**                                |
| :--------- | :------- | :--------------------------------------------- |
| K          | Call     | Send a request within a session                |
| T          | Status   | Request or report session state                |
| N          | Notify   | Push an event notification within a session   |
| L          | Locate   | Find which bus a session (SID) resides on     |
| U          | Resume   | Reconnect to an interrupted session            |
| F          | Finish   | Close a session                                |

---

# 9. Bus Operations

## 9.1 BID Establishment

When an instance connects to a bus, it must obtain a unique BID through the Establish/Conflict protocol.

### 9.1.1 Establishment Procedure

1. Generate a random BID candidate of the initial length (base-32 encoded).
2. Transmit an Establish message with the candidate BID.
3. Wait for the timeout period.
4. If a Conflict is received for this BID, increment the BID length and return to step 1.
5. If the BID length exceeds the maximum, transmit an Exception with reason "BID_OVERFLOW" and abort the connection attempt.
6. If no Conflict is received within the timeout, the BID is claimed. The instance begins listening on the bus.

### 9.1.2 Conflict Detection

While listening on a bus, an instance that receives an Establish message for a BID it already holds must respond with a Conflict message containing that BID.

### 9.1.3 State Machine

| **State**   | **Entry Condition**        | **Action**             | **Transitions**                                                                                              |
| :---------- | :------------------------- | :--------------------- | :----------------------------------------------------------------------------------------------------------- |
| IDLE        | Initial / after disconnect | None                   | PROPOSING (on connect)                                                                                       |
| PROPOSING   | Generated candidate BID    | Send !E, start timeout | ESTABLISHED (timeout expires, no conflict) / PROPOSING (conflict received, length < max) / FAILED (length >= max) |
| ESTABLISHED | No conflict within timeout | Listen on bus          | IDLE (on disconnect)                                                                                         |
| FAILED      | Max BID length exceeded    | Send !X "BID_OVERFLOW" | IDLE                                                                                                         |

### 9.1.4 Wire Examples

**Establish BID:**

```
Pseudo:  [SOM] [SOW]!E[EOW] [SOW]@[SOR]U[SOB]4T9X2[EOW] [EOM]
Wire:    ☻↕!→E►↕@♦U→4T9X2►♥
```
*Instance proposes BID "4T9X2" on the bus.*

**Conflict detected:**

```
Pseudo:  [SOM] [SOW]!C[EOW] [SOW]@[SOR]U[SOB]4T9X2[EOW] [EOM]
Wire:    ☻↕!→C►↕@♦U→4T9X2►♥
```
*Existing instance reports BID "4T9X2" is already in use.*

**Establishment failed:**

```
Pseudo:  [SOM] [SOW]!X[EOW] [SOW]"[SOB]BID_OVERFLOW[EOW] [EOM]
Wire:    ☻↕!→X►↕"→BID_OVERFLOW►♥
```
*Instance could not establish a BID within the maximum length.*

## 9.2 Reactive Scaleback

Scaleback is the mechanism by which a constrained receiver communicates its limits to a sender. There is no upfront capability handshake. A receiver that cannot process a message has two options: send a Scaleback response stating its limits, or silently drop the message.

### 9.2.1 Drop (Silent)

A receiver that cannot process a message and cannot (or chooses not to) send a Scaleback simply discards the remaining frame bytes and scans forward for the next SOM byte. This is the universal fallback that requires zero transmit capability. The sender receives no indication that the message was dropped.

### 9.2.2 Scaleback Message

A Scaleback message communicates receiver limits using two mechanisms:

- **Negative flags** (LOGICAL word): An exclusion list of radix types and/or unit types the receiver cannot handle, expressed as a logical negation.
- **Positive sizes** (INTEGER words): Maximum head size in bytes, followed by maximum tail size in bytes. A tail size of 0 means the receiver cannot handle binary tails.

A fully capable device never sends Scaleback. Only constraints are communicated. Either component (flags or sizes) may be omitted if that dimension is unconstrained.

### 9.2.3 Scaleback Wire Examples

**Full Scaleback (type exclusions + size limits):**

```
Pseudo:  [SOM] [SOW]!S[EOW] [SOW]?[SOB]!H&!Q[EOW] [SOW]#[SOR]D[SOU]W[SOB]32[EOW] [SOW]#[SOR]D[SOU]W[SOB]0[EOW] [EOM]
Wire:    ☻↕!→S►↕?→!H&!Q►↕#♦D•W→32►↕#♦D•W→0►♥
```
*Cannot handle hex or quadword. Max head 32 bytes. No tail.*

**Size-only Scaleback (no type exclusions):**

```
Pseudo:  [SOM] [SOW]!S[EOW] [SOW]#[SOR]D[SOU]W[SOB]64[EOW] [SOW]#[SOR]D[SOU]W[SOB]0[EOW] [EOM]
Wire:    ☻↕!→S►↕#♦D•W→64►↕#♦D•W→0►♥
```
*No type exclusions. Max head 64 bytes. No tail.*

**Flags-only Scaleback (no size limits):**

```
Pseudo:  [SOM] [SOW]!S[EOW] [SOW]?[SOB]!Q&!T[EOW] [EOM]
Wire:    ☻↕!→S►↕?→!Q&!T►♥
```
*Cannot handle quadword or terabyte. No size constraints.*

On receiving a Scaleback, the sender may resend the message within the stated limits, attempt a different encoding, or accept that this receiver cannot handle the message. The sender has no obligation to retry.

### 9.2.4 Scaleback Semantics

- A Scaleback applies to the sender-receiver pair for the duration of the current bus connection.
- A receiver may send a new Scaleback at any time to update its limits (e.g., after freeing buffer space).
- A Scaleback with no LOGICAL word and no INTEGER words is a no-op and should be ignored.
- Scaleback is unreliable by design. There is no guaranteed delivery. A sender that receives a Scaleback may respond with an Acknowledge (`!W`) containing the Scaleback sender's BID, confirming the limits were received. The Acknowledge carries only the BID; correlation is implicit — an Acknowledge from BID X to BID Y confirms receipt of the most recent Scaleback from Y to X. A receiver that does not receive an Acknowledge MAY retransmit the Scaleback. A sender that does not respect stated limits after acknowledging them will have its messages silently dropped.

## 9.3 Path Addressing

Antheos uses a path-and-index mechanism for multi-hop routing. A PATH word contains a dot-separated sequence of BIDs representing the complete route. An INTEGER index word identifies the current position within that path. The index is zero-based: position 0 is the origin, position N is the Nth hop.

When paths are used for addressing, the message ends with the index followed by the path. This ordering is intentional: a stream-parsing instance reads the index first, then scans the path counting dots to find its target position. Single-pass, no backtracking, no buffering the entire path.

```
... [payload words] [#index] [/path] [EOM]
```

### 9.3.1 Broadcast Path (Expanding)

A broadcast carries a single path that expands as the message traverses instances. Each forwarding instance appends its own BID to the path. No index is needed during broadcast — the message goes to all instances.

When any receiver wants to reply, the path contains the complete return route. The receiver sets the index to its own position (the last entry) and sends a Relay using the same path.

**Trace: Broadcast from A (BID=AA) through B, C to D:**

At A (origin):
```
Pseudo:  [SOM] [SOW]!B[EOW] [SOW]"[SOB]Hello[EOW] [SOW]/[SOB]AA[EOW] [EOM]
Wire:    ☻↕!→B►↕"→Hello►↕/→AA►♥
```
*Origin broadcasts. Path contains only the origin BID.*

At B (appends BB):
```
Pseudo:  [SOM] [SOW]!B[EOW] [SOW]"[SOB]Hello[EOW] [SOW]/[SOB]AA.BB[EOW] [EOM]
Wire:    ☻↕!→B►↕"→Hello►↕/→AA.BB►♥
```
*B forwards broadcast, appending its BID to the path.*

At C (appends CC):
```
Pseudo:  [SOM] [SOW]!B[EOW] [SOW]"[SOB]Hello[EOW] [SOW]/[SOB]AA.BB.CC[EOW] [EOM]
Wire:    ☻↕!→B►↕"→Hello►↕/→AA.BB.CC►♥
```
*C forwards broadcast, appending its BID.*

At D (appends DD):
```
Pseudo:  [SOM] [SOW]!B[EOW] [SOW]"[SOB]Hello[EOW] [SOW]/[SOB]AA.BB.CC.DD[EOW] [EOM]
Wire:    ☻↕!→B►↕"→Hello►↕/→AA.BB.CC.DD►♥
```
*D receives broadcast. Path is now complete and fixed.*

### 9.3.2 Relay Path (Index Decrement)

A relay carries the complete fixed path (established during broadcast or known in advance) and a zero-based index indicating the current sender's position. Each forwarding instance decrements the index and forwards to the BID at position index–1 in the path.

The path never changes during relay. Only the index is modified. This means a forwarding instance performs no string manipulation — it reads the index, subtracts one, counts dots in the path to find the next-hop BID, and forwards.

**Relay procedure at each hop:**

1. Read the index from the INTEGER word.
2. Compute next_hop = index − 1.
3. If next_hop < 0: error, discard message.
4. Find path[next_hop] by counting dots (zero-based) in the PATH body.
5. If path[next_hop] matches my BID: I am the destination. Deliver the message.
6. Otherwise: write next_hop as the new index, forward to path[next_hop].

**Trace: D (index=3) replies to A via the established path AA.BB.CC.DD:**

D sends (index=3, D's position):
```
Pseudo:  [SOM] [SOW]!R[EOW] [SOW]"[SOB]Reply[EOW] [SOW]#[SOR]D[SOU]B[SOB]3[EOW] [SOW]/[SOB]AA.BB.CC.DD[EOW] [EOM]
Wire:    ☻↕!→R►↕"→Reply►↕#♦D•B→3►↕/→AA.BB.CC.DD►♥
```
*D sends relay. Index=3, next hop is path[2]=CC.*

At C (decrements to 2):
```
Pseudo:  [SOM] [SOW]!R[EOW] [SOW]"[SOB]Reply[EOW] [SOW]#[SOR]D[SOU]B[SOB]2[EOW] [SOW]/[SOB]AA.BB.CC.DD[EOW] [EOM]
Wire:    ☻↕!→R►↕"→Reply►↕#♦D•B→2►↕/→AA.BB.CC.DD►♥
```
*C forwards. Index=2, next hop is path[1]=BB.*

At B (decrements to 1):
```
Pseudo:  [SOM] [SOW]!R[EOW] [SOW]"[SOB]Reply[EOW] [SOW]#[SOR]D[SOU]B[SOB]1[EOW] [SOW]/[SOB]AA.BB.CC.DD[EOW] [EOM]
Wire:    ☻↕!→R►↕"→Reply►↕#♦D•B→1►↕/→AA.BB.CC.DD►♥
```
*B forwards. Index=1, next hop is path[0]=AA.*

At A (index=1, path[0]=AA = me): message delivered.

*A receives the message. Index-1=0, path[0]=AA matches own BID. Delivered.*

### 9.3.3 Path Recovery

**Failure Detection:** A forwarding instance that cannot deliver to the next-hop BID (instance disconnected, bus lost, timeout) MUST NOT forward the message. It becomes the recovery origin for that message.

**Recovery:** The recovery origin issues a Discover (`!D`) for the unreachable BID, broadcasting it on all buses it is connected to. If a Discover response is received within timeout, the recovery origin updates its local routing table and retransmits the original message toward the newly discovered BID. The path word is not modified — path integrity is the originator's concern.

**Wire example** (C cannot reach BB):
```
Pseudo:  [SOM] [SOW]!D[EOW] [SOW]@[SOR]U[SOB]BB[EOW] [EOM]
Wire:    ☻↕!→D►↕@♦U→BB►♥
```
*C broadcasts Discover for BID BB.*

**Discover Fails:** If no Discover response is received within timeout, the recovery origin sends `!X RELAY_FAILED` back toward the originator via the reverse relay procedure — index set to the recovery origin's own position, decrementing toward position 0. The Exception carries the unreachable BID as a TEXT word.

**Return Path Also Broken:** If the recovery origin cannot relay the Exception back, it MUST silently discard the message. The originator will detect failure through its own timeout.

## 9.4 Broadcast

A broadcast message is sent to all instances on the current bus. Broadcast is always a local bus operation. Multi-bus forwarding is handled by Relay (Section 9.3.2), using the path built during broadcast (Section 9.3.1).

```
Pseudo:  [SOM] [SOW]!B[EOW] [SOW]"[SOB]Hello all[EOW] [EOM]
Wire:    ☻↕!→B►↕"→Hello all►♥
```
*Broadcast to all instances on this bus.*

## 9.5 Ping

Ping is a keepalive and presence check. It may target a specific BID or be broadcast. The response is a Ping containing the responder's BID.

**Ping a specific instance:**

```
Pseudo:  [SOM] [SOW]!P[EOW] [SOW]@[SOR]U[SOB]4T9X2[EOW] [EOM]
Wire:    ☻↕!→P►↕@♦U→4T9X2►♥
```
*Ping instance at BID "4T9X2".*

**Response:**

```
Pseudo:  [SOM] [SOW]!P[EOW] [SOW]@[SOR]U[SOB]4T9X2[EOW] [EOM]
Wire:    ☻↕!→P►↕@♦U→4T9X2►♥
```
*Pong: instance 4T9X2 confirms presence.*

**Broadcast ping:**

```
Pseudo:  [SOM] [SOW]!P[EOW] [EOM]
Wire:    ☻↕!→P►♥
```
*Broadcast ping. All instances respond with their BID.*

**Broadcast responses:**

```
Wire:    ☻↕!→P►↕@♦U→4T9X2►♥
Wire:    ☻↕!→P►↕@♦U→7M3K9►♥
```
*Multiple instances respond, each with own BID.*

## 9.6 Discover

Discover (`!D`) finds which bus a BID resides on. This is used for path recovery when a relay cannot reach the next hop.

**Request:**

```
Pseudo:  [SOM] [SOW]!D[EOW] [SOW]@[SOR]U[SOB]4T9X2[EOW] [EOM]
Wire:    ☻↕!→D►↕@♦U→4T9X2►♥
```
*Discover: where is BID 4T9X2?*

**Response (BID found on this bus):**

```
Pseudo:  [SOM] [SOW]!D[EOW] [SOW]@[SOR]U[SOB]4T9X2[EOW] [SOW]@[SOR]U[SOB]7M3K9[EOW] [EOM]
Wire:    ☻↕!→D►↕@♦U→4T9X2►↕@♦U→7M3K9►♥
```
*Discovered: BID 4T9X2 is reachable via BID 7M3K9 on this bus.*

An instance that receives a Discover for a BID it holds responds with its own BID. An instance connected to multiple buses that knows the target BID is reachable on another bus MAY respond with the gateway BID for that route. If no response is received, the BID is considered unreachable.

## 9.7 Verify

Verify resolves a BID to its full OID:DID:IID identity. The response contains the three identity components as separate ID words.

**Request:**

```
Pseudo:  [SOM] [SOW]!V[EOW] [SOW]@[SOR]U[SOB]4T9X2[EOW] [EOM]
Wire:    ☻↕!→V►↕@♦U→4T9X2►♥
```
*Request identity of instance at BID "4T9X2".*

**Response:**

```
Pseudo:  [SOM] [SOW]!V[EOW] [SOW]@[SOB]langsyn[EOW] [SOW]@[SOB]Thermostat[EOW] [SOW]@[SOB]SN00482[EOW] [EOM]
Wire:    ☻↕!→V►↕@→langsyn►↕@→Thermostat►↕@→SN00482►♥
```
*Response: OID=langsyn, DID=Thermostat, IID=SN00482.*

## 9.8 Acknowledge

Acknowledge (`!W`) confirms receipt of a bus-scope message. It is used primarily to confirm Scaleback was received, but may be used to confirm any bus-scope message where the sender requires assurance.

```
Pseudo:  [SOM] [SOW]!W[EOW] [SOW]@[SOR]U[SOB]4T9X2[EOW] [EOM]
Wire:    ☻↕!→W►↕@♦U→4T9X2►♥
```
*Acknowledge: confirming receipt of a message from BID "4T9X2".*

Acknowledge is not mandatory for any message. It is available when confirmation is needed.

## 9.9 Exception

Exceptions are structured error reports. The SYMBOL !X is followed by a TEXT word containing the error reason.

```
Pseudo:  [SOM] [SOW]!X[EOW] [SOW]"[SOB]BID_OVERFLOW[EOW] [EOM]
Wire:    ☻↕!→X►↕"→BID_OVERFLOW►♥
```
*Exception: BID establishment exceeded maximum length.*

---

# 10. Service Operations

Service discovery follows a strict Query/Offer/Accept pattern. An instance that needs a capability queries for it. Instances that can provide it respond with offers. The querying instance selects and accepts an offer.

Sessions are not obligatory. A service interaction may consist of a single Query/Offer/Accept exchange followed by direct message exchange, or it may establish a long-lived session. The service scope defines the discovery mechanism; the session scope (Section 11) defines optional persistent communication.

## 10.1 Query

An instance broadcasts or directs a query describing the service it needs. The query contains a TEXT word describing the desired capability.

```
Pseudo:  [SOM] [SOW]!Q[EOW] [SOW]"[SOB]temperature[EOW] [EOM]
Wire:    ☻↕!→Q►↕"→temperature►♥
```
*Query: I need a temperature service.*

## 10.2 Offer

Instances that can provide the requested service respond with an offer. The offer includes the responding instance's BID and a description of what it provides. The offer implicitly defines the interface — there is no separate interface definition language.

```
Pseudo:  [SOM] [SOW]!O[EOW] [SOW]@[SOR]U[SOB]4T9X2[EOW] [SOW]"[SOB]temperature_celsius[EOW] [EOM]
Wire:    ☻↕!→O►↕@♦U→4T9X2►↕"→temperature_celsius►♥
```
*Offer: instance at BID "4T9X2" can provide temperature in Celsius.*

## 10.3 Accept

The querying instance accepts an offer by addressing the offering instance's BID. This establishes the service agreement and may optionally initiate a session.

```
Pseudo:  [SOM] [SOW]!A[EOW] [SOW]@[SOR]U[SOB]4T9X2[EOW] [EOM]
Wire:    ☻↕!→A►↕@♦U→4T9X2►♥
```
*Accept: I accept the offer from BID "4T9X2".*

### 10.3.1 Accept with Sender BID

An Accept frame MUST include a second `@U` word carrying the sender's BID. This allows the offering instance to identify who accepted its offer without waiting for a session to be established.

```
Pseudo:  [SOM] [SOW]!A[EOW] [SOW]@[SOR]U[SOB]4T9X2[EOW] [SOW]@[SOR]U[SOB]7M3K9[EOW] [EOM]
Wire:    ☻↕!→A►↕@♦U→4T9X2►↕@♦U→7M3K9►♥
```
*Accept: instance "7M3K9" accepts the offer from BID "4T9X2".*

The first `@U` word is the target BID (the offering instance). The second `@U` word is the sender BID (the accepting instance).

## 10.4 Offer Collection

A Query may elicit multiple Offers from different instances. The querying instance decides how to handle them:

- **First-offer-wins**: Accept the first Offer received, ignore subsequent Offers. Minimizes latency.
- **Collect-then-choose**: Wait for a timeout period, collect all Offers, then Accept the preferred one. Enables comparison.

Offers not followed by an Accept are implicitly declined. No explicit rejection message is required. An offering instance MUST NOT assume its Offer will be accepted and MUST NOT allocate resources until Accept is received.

---

# 11. Session Operations

Sessions provide persistent, stateful communication between instances. They are optional — bus-scope operations and one-shot service interactions do not require sessions. When an interaction needs message sequencing, state tracking, or long-lived communication, a session is established.

## 11.1 Session Lifecycle

### 11.1.1 Creation

A session is typically initiated after a service Accept, but may be opened directly between instances that already know each other. The initiating instance generates a fresh SID from a hash of OID:DID:IID:\<counter\> and sends the first session-scope message.

### 11.1.2 Active Communication

Within a session, messages are sequenced by MID, a decimal counter starting at 1 and incrementing by 1 with each message. Call (`!K`) sends requests, Status (`!T`) queries state, and Notify (`!N`) pushes unsolicited events.

**MID Wrap:** When MID reaches the maximum value, the instance transmits a wrap message with MID=0 — containing only the session verb, SID, and MID=0, with no payload. The receiver resets its sequence expectation to MID=1. Normal sequencing resumes immediately after the wrap message.

### 11.1.3 Interruption and Recovery

If a session is interrupted (bus disconnect, device reset), the Locate verb finds where the remote instance now resides, and Resume reconnects to the session.

### 11.1.4 Termination

Finish closes a session. The SID is discarded — it is never reused.

### 11.1.5 State Machine

| **State**  | **Entry Condition**          | **Transitions**                                       |
| :--------- | :--------------------------- | :---------------------------------------------------- |
| IDLE       | No session                   | ACTIVE (on Accept or direct open)                     |
| ACTIVE     | Session created, SID assigned| SUSPENDED (on disconnect) / IDLE (on Finish)          |
| SUSPENDED  | Bus lost or timeout          | ACTIVE (on Resume) / IDLE (on Finish or timeout expiry)|

**Session Timeout:** The duration before a SUSPENDED session transitions to IDLE is implementation-defined. An instance whose session has timed out MUST respond with `!X SESSION_EXPIRED` if a Resume (`!U`) arrives for that SID afterward. An instance MAY signal an impending timeout via `!T` (Status) before transitioning.

## 11.2 SID Generation

SIDs are variable-length base-32 hashes derived from the instance's identity and a monotonic counter. Every session receives a fresh SID — SIDs are never reused. SID length starts short and grows on collision, keeping typical messages compact while scaling to high session counts.

**SID Length:** An implementation defines initial length, increment on collision, and maximum length. SID generation starts at the initial length and grows on collision until a unique SID is found or maximum length is reached.

**Generation Procedure:**

1. Compute hash of OID:DID:IID:\<sid_counter\>. Store hash bytes in little-endian order (least significant byte first) so that truncation preserves the most-varying bits.
2. Encode as base-32, truncated to current SID length (starting at initial length).
3. Check if truncated SID matches any currently active session.
4. If collision: increase length by increment, re-truncate, and repeat from step 3.
5. If length exceeds maximum: increment sid_counter, reset length to initial, and repeat from step 1.
6. If no collision: SID is assigned, increment sid_counter.

**Rotation:**

SIDs are generated fresh for every session. When a session finishes, its SID is discarded. The monotonic counter ensures forward progress — no SID is ever reissued. The counter resets on device reboot.

If a Resume (`!U`) arrives for a SID that is currently active but belongs to a different session than the sender expects, the receiving instance MUST respond with `!X RESUME_DENIED`.

## 11.3 Call

Call sends a request within an active session and expects a response. The response is itself a Call with the next sequential MID. Receipt of the response confirms delivery of the request. If no response arrives within timeout, the sender MAY retransmit or treat the session as interrupted.

**Request:**

```
Pseudo:  [SOM] [SOW]!K[EOW] [SOW]@[SOR]U[SOB]A7K2M[EOW] [SOW]@[SOR]D[SOB]1[EOW] [SOW]"[SOB]get_reading[EOW] [EOM]
Wire:    ☻↕!→K►↕@♦U→A7K2M►↕@♦D→1►↕"→get_reading►♥
```
*Session Call: SID=A7K2M, MID=1, request="get_reading".*

**Response:**

```
Pseudo:  [SOM] [SOW]!K[EOW] [SOW]@[SOR]U[SOB]A7K2M[EOW] [SOW]@[SOR]D[SOB]2[EOW] [SOW]"[SOB]23.5[EOW] [EOM]
Wire:    ☻↕!→K►↕@♦U→A7K2M►↕@♦D→2►↕"→23.5►♥
```
*Session Call response: SID=A7K2M, MID=2, response="23.5".*

## 11.4 Status

Status requests or reports the current state of a session. A Status request expects a Status response; the response confirms both delivery and current session state.

**Request:**

```
Pseudo:  [SOM] [SOW]!T[EOW] [SOW]@[SOR]U[SOB]A7K2M[EOW] [SOW]@[SOR]D[SOB]2[EOW] [EOM]
Wire:    ☻↕!→T►↕@♦U→A7K2M►↕@♦D→2►♥
```
*Session Status request: SID=A7K2M, MID=2.*

**Response:**

```
Pseudo:  [SOM] [SOW]!T[EOW] [SOW]@[SOR]U[SOB]A7K2M[EOW] [SOW]@[SOR]D[SOB]3[EOW] [SOW]"[SOB]ACTIVE[EOW] [EOM]
Wire:    ☻↕!→T►↕@♦U→A7K2M►↕@♦D→3►↕"→ACTIVE►♥
```
*Session Status response: SID=A7K2M, MID=3, state="ACTIVE".*

## 11.5 Notify

Notify pushes an unsolicited event within a session. Unlike Call, it does not expect a response and delivery is not confirmed. Notify is fire-and-forget; use Call if acknowledgment is required.

```
Pseudo:  [SOM] [SOW]!N[EOW] [SOW]@[SOR]U[SOB]A7K2M[EOW] [SOW]@[SOR]D[SOB]3[EOW] [SOW]"[SOB]threshold_exceeded[EOW] [EOM]
Wire:    ☻↕!→N►↕@♦U→A7K2M►↕@♦D→3►↕"→threshold_exceeded►♥
```
*Session Notify: SID=A7K2M, MID=3, event="threshold_exceeded".*

## 11.6 Locate

Locate finds which bus a session (SID) resides on. This is used after a bus change or reconnect.

**Request:**

```
Pseudo:  [SOM] [SOW]!L[EOW] [SOW]@[SOR]U[SOB]A7K2M[EOW] [EOM]
Wire:    ☻↕!→L►↕@♦U→A7K2M►♥
```
*Locate: where is session A7K2M?*

**Response (session found):**

```
Pseudo:  [SOM] [SOW]!L[EOW] [SOW]@[SOR]U[SOB]A7K2M[EOW] [SOW]@[SOR]U[SOB]4T9X2[EOW] [EOM]
Wire:    ☻↕!→L►↕@♦U→A7K2M►↕@♦U→4T9X2►♥
```
*Located: session A7K2M is at BID 4T9X2.*

## 11.7 Resume

Resume reconnects to an interrupted session. The remote instance responds with Status confirming the session state, or with an Exception if the session is no longer available.

**Request:**

```
Pseudo:  [SOM] [SOW]!U[EOW] [SOW]@[SOR]U[SOB]A7K2M[EOW] [EOM]
Wire:    ☻↕!→U►↕@♦U→A7K2M►♥
```
*Resume session A7K2M.*

**Response (success):**

```
Pseudo:  [SOM] [SOW]!T[EOW] [SOW]@[SOR]U[SOB]A7K2M[EOW] [SOW]@[SOR]D[SOB]47[EOW] [SOW]"[SOB]ACTIVE[EOW] [EOM]
Wire:    ☻↕!→T►↕@♦U→A7K2M►↕@♦D→47►↕"→ACTIVE►♥
```
*Session resumed: SID=A7K2M, last MID=47, state=ACTIVE. Normal communication resumes at MID 48.*

**Response (failure):**

```
Pseudo:  [SOM] [SOW]!X[EOW] [SOW]"[SOB]SESSION_EXPIRED[EOW] [EOM]
Wire:    ☻↕!→X►↕"→SESSION_EXPIRED►♥
```
*Session A7K2M has timed out and been reclaimed.*

## 11.8 Finish

Finish closes a session. The sender immediately transitions the session to IDLE and discards the SID. Finish is fire-and-forget — no response is expected. If the remote instance does not receive the Finish, its session timeout will eventually reclaim the session.

```
Pseudo:  [SOM] [SOW]!F[EOW] [SOW]@[SOR]U[SOB]A7K2M[EOW] [EOM]
Wire:    ☻↕!→F►↕@♦U→A7K2M►♥
```
*Finish session A7K2M. SID discarded.*

---

# 12. Bus Transmission Rules

- Buses carry CP437 byte streams transparently. A bus does not interpret, modify, or filter message content.
- Devices respect PATH words for routing. A device that receives a message with a PATH not addressed to it forwards the message if connected to the next hop, or discards it.
- No session or message interpretation occurs at the bus level. The bus is a passive transport medium.
- Bytes between messages (outside SOM/EOM framing) are ignored. This allows Antheos to coexist with other traffic on shared buses.

---

# 13. Standard Exception Codes

The following exception reason strings are defined for Level 1. Implementations may define additional reason strings for application-specific errors. Exception reasons are carried in a TEXT word following the !X symbol.

| **Reason**          | **Scope** | **Meaning**                                      |
| :------------------ | :-------- | :----------------------------------------------- |
| BID_OVERFLOW        | Bus       | BID establishment exceeded maximum length        |
| BID_TIMEOUT         | Bus       | BID establishment timed out without resolution   |
| RELAY_FAILED        | Bus       | Message could not be forwarded to the next hop   |
| PATH_BROKEN         | Bus       | A hop in the route path is unreachable           |
| INDEX_INVALID       | Bus       | Path index is out of range for the given path    |
| UNKNOWN_BID         | Bus       | Addressed BID not found on this bus              |
| MALFORMED_FRAME     | Bus       | Message frame is structurally invalid            |
| UNSUPPORTED_TYPE    | Bus       | Word type not supported by this instance         |
| SERVICE_UNKNOWN     | Service   | No instance offers the requested service         |
| OFFER_EXPIRED       | Service   | The offer is no longer valid                     |
| SESSION_NOT_FOUND   | Session   | SID does not match any known session             |
| SESSION_EXPIRED     | Session   | Session has timed out and been reclaimed         |
| RESUME_DENIED       | Session   | Session cannot be resumed                        |
| MID_OUT_OF_ORDER    | Session   | Message ID does not follow expected sequence     |

---

# 14. Capability

Level 1 is the complete, self-contained baseline. Every symbol has a single, fixed meaning. Every message does exactly one thing. This is the universal foundation.

Capability tiers beyond Level 1 are discovered at runtime through the Query/Offer/Accept mechanism (Section 10) and defined in separate specifications. Level 1 makes no assumptions about what those capabilities are or how they behave.

---

# 15. Conformance Requirements

This section defines the requirements for a conformant Antheos Level 1 implementation. The keyword MUST is used per RFC 2119.

## 15.1 Minimum Conformant Implementation

An instance MUST:

- Parse SOM and EOM to identify message boundaries.
- Parse SOW and EOW to identify word boundaries.
- Recognize the SYMBOL word type (!).
- Implement the Establish/Conflict protocol (bus scope: E, C).
- Silently drop messages it cannot process (scan forward to next SOM).

## 15.2 Optional Features

An instance MAY implement any combination of:

- Bus scope: B, P, R, D, V, S, W, X
- Service scope: Q, O, A
- Session scope: K, T, N, L, U, F
- Path addressing and relay
- Any word types beyond SYMBOL

---

# 16. Security Considerations

Antheos Level 1 provides no encryption, authentication, or integrity protection. The protocol is designed as a transparent transport layer; security is a concern for higher layers built on top of Level 1.

- **No encryption**: All message content is transmitted in cleartext CP437. Any entity with access to the bus can read all traffic.
- **No authentication**: BIDs are self-asserted through Establish/Conflict. Any instance can claim any available BID. The Verify verb provides identity disclosure but not identity proof.
- **No integrity**: Messages can be modified in transit on shared buses. There is no checksum or signature mechanism at Level 1.
- **BID spoofing**: An adversary can claim a BID that was previously held by another instance after that instance disconnects.

Applications requiring security implement encryption and authentication as a higher-level service, negotiated through Query/Offer/Accept. The Antheos frame structure (typed words, delimited boundaries) provides clear attachment points for security wrappers around message content.

SIDs are generated fresh per session and never reused, preventing cross-session correlation through SID observation. However, the SID hash derivation (from OID:DID:IID:\<counter\>) is deterministic and not cryptographic — an observer who knows the identity and counter can predict future SIDs. Knowledge of a SID allows interaction with the session.

---

# 17. Licensing

Copyright (c) 2025-2026 Are Bjorby <are.bjorby@proton.me>

All rights reserved. This specification and associated documentation are proprietary.
No part may be used, copied, modified, merged, published, distributed, sublicensed,
or sold without the prior written permission of the copyright holder.

---

*— End of Antheos Protocol 1.0 Level 1 Specification —*
