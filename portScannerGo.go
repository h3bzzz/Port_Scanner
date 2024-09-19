package main

import (
	"flag"
	"fmt"
	"net"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"
)

type Options struct {
	targets []string
	ports   []string
}

// Parse the Args
func parseArgs() Options {
	var targets, ports string
	flag.StringVar(&targets, "t", "", "Targets to scan (comma separated)")
	flag.StringVar(&ports, "p", "80,443", "Ports to scan (comma separated)")
	flag.Parse()

	if targets == "" {
		fmt.Printf("Usage: %s -t <target> -p <port>\n", os.Args[0])
		os.Exit(1)
	}

	// Split targets and ports
	return Options{
		targets: strings.Split(targets, ","),
		ports:   strings.Split(ports, ","),
	}
}

// Resolve the IPs
func resolve(target string) ([]string, error) {
	addrs, err := net.LookupIP(target)
	if err != nil {
		return nil, fmt.Errorf("failed to resolve %s: %v", target, err)
	}

	var ipList []string
	for _, addr := range addrs {
		if ipv4 := addr.To4(); ipv4 != nil {
			ipList = append(ipList, ipv4.String())
		} else if ipv6 := addr.To16(); ipv6 != nil {
			ipList = append(ipList, ipv6.String())
		}
	}
	if len(ipList) == 0 {
		return nil, fmt.Errorf("no valid IP found for %s", target)
	}
	return ipList, nil
}

// Parse Ports
func parsePorts(ports []string) []int {
	var portList []int
	for _, portInput := range ports {
		if portInput == "common" {
			// Add common ports
			commonPorts := []int{20, 21, 22, 23, 25, 53, 67, 68, 69, 80, 110, 123, 135, 137, 138, 139, 143, 161, 162, 179, 194, 389, 443, 445, 465, 514, 515, 587, 993, 995, 1433, 1434, 1521, 1723, 2049, 2083, 2087, 3128, 3306, 3389, 5432, 5900, 5985, 5986, 6379, 8080, 8443, 8888, 9090, 9200, 10000, 27017}
			portList = append(portList, commonPorts...)
		} else if strings.Contains(portInput, "-") {
			ranges := strings.Split(portInput, "-")
			start, err := strconv.Atoi(ranges[0])
			if err != nil {
				fmt.Printf("Failed to parse port %s: %v\n", ranges[0], err)
				continue
			}
			end, err := strconv.Atoi(ranges[1])
			if err != nil {
				fmt.Printf("Failed to parse port %s: %v\n", ranges[1], err)
				continue
			}
			for i := start; i <= end; i++ {
				portList = append(portList, i)
			}
		} else {
			port, err := strconv.Atoi(portInput)
			if err != nil {
				fmt.Printf("Failed to parse port %s: %v\n", portInput, err)
				continue
			}
			portList = append(portList, port)
		}
	}
	return portList
}

// Scan the Ports
func scanPort(target string, port int, timeout time.Duration) bool {
	address := fmt.Sprintf("%s:%d", target, port)
	conn, err := net.DialTimeout("tcp", address, timeout)
	if err != nil {
		return false
	}
	conn.Close()
	return true
}

// So here is where C has its Threads and Go has its GoRoutines
func threadScan(target string, port int, wg *sync.WaitGroup) {
	defer wg.Done()
	if scanPort(target, port, 5*time.Second) {
		fmt.Printf("%s:%d is open\n", target, port)
	}
}

func main() {
	options := parseArgs()

	// Resolve
	var ips []string
	for _, target := range options.targets {
		ipList, err := resolve(target)
		if err != nil {
			fmt.Printf("Failed to resolve %s: %v\n", target, err)
			continue
		}
		ips = append(ips, ipList...)
	}

	// Parse Ports
	ports := parsePorts(options.ports)

	// Scan
	var wg sync.WaitGroup
	for _, ip := range ips {
		for _, port := range ports {
			wg.Add(1)
			go threadScan(ip, port, &wg)
		}
	}
	wg.Wait()
}


// Huge thanks to 
https://medium.com/@KentGruber/building-a-high-performance-port-scanner-with-golang-9976181ec39d
This article helped so much in putting some of the basic pieces together. The rest I got off
the amazing resources Golang.org/x/... has 
