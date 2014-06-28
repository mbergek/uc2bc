# uc2bc

Sometimes it might be beneficial to redistribute unicast UDP traffic to multiple other clients. This program provides a solution to this problem. It listens on a specified UDP port on all local addresses and resends those packets to the local broadcast address. To avoid creating a loop the incoming and outgoing ports must be different. The application can also ensure that only packets coming from a specified source are being retransmitted. While the latter doesn’t provide any real security since it is trivial to spoof the source IP address, it does provide some immunity from other applications.


```
Usage:
  uc2bc [options]

Options:
  --listen-port -i [port]       # The port to listen for incoming packets on
  --broadcast-address -b [addr] # The local broadcast address
  --broadcast-port -o [port]    # The port to use for outgoing packets
  --source-address -s [addr]    # The source for authorised packets

Example:
  uc2bc -i 5000 -b 192.168.0.255 -o 5001
  
```

## Compile

```
gcc -o uc2bc uc2bc.c
```

## Acknowledgements
Thanks to Andreas who got me (re)started with socket programming on *NIX.