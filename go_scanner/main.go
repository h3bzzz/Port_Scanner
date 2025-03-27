package main

import (
	"flag"
	"fmt"
	"net"
	"os"
	"runtime"
	"strconv"
	"strings"
	"sync"
	"time"
)

type Options struct {
	targets     []string
	ports       []string
	timeout     time.Duration
	maxThreads  int
	verbose     bool
	showVersion bool
}

const (
	defaultTimeout    = 1000 * time.Millisecond
	defaultMaxThreads = 100
	version           = "1.0.0"
)

func parseArgs() Options {
	var targets, ports string
	var timeout int
	var maxThreads int
	var verbose, showVersion bool

	flag.StringVar(&targets, "t", "", "Targets to scan (comma separated)")
	flag.StringVar(&ports, "p", "80,443", "Ports to scan (comma separated, or 'common' or 'all')")
	flag.IntVar(&timeout, "T", int(defaultTimeout/time.Millisecond), "Connection timeout in milliseconds")
	flag.IntVar(&maxThreads, "j", defaultMaxThreads, "Maximum number of concurrent threads")
	flag.BoolVar(&verbose, "v", false, "Verbose mode (show closed ports)")
	flag.BoolVar(&showVersion, "V", false, "Show version information")
	flag.Parse()

	if showVersion {
		fmt.Printf("Port Scanner version %s\n", version)
		os.Exit(0)
	}

	if targets == "" {
		printUsage()
		os.Exit(1)
	}

	return Options{
		targets:    strings.Split(targets, ","),
		ports:      strings.Split(ports, ","),
		timeout:    time.Duration(timeout) * time.Millisecond,
		maxThreads: maxThreads,
		verbose:    verbose,
	}
}

func printUsage() {
	fmt.Printf("Usage: %s [OPTIONS]\n\n", os.Args[0])
	fmt.Println("OPTIONS:")
	fmt.Println("  -t <targets>    Targets to scan (comma separated)")
	fmt.Println("  -p <ports>      Ports to scan (comma separated, or 'common' or 'all')")
	fmt.Println("                  Default: 80,443")
	fmt.Println("  -T <timeout>    Connection timeout in milliseconds")
	fmt.Println("                  Default: 1000")
	fmt.Println("  -j <threads>    Maximum number of concurrent threads")
	fmt.Printf("                  Default: %d\n", defaultMaxThreads)
	fmt.Println("  -v              Verbose mode (show closed ports)")
	fmt.Println("  -V              Show version information")
	fmt.Println("\nExamples:")
	fmt.Printf("  %s -t example.com -p 80,443\n", os.Args[0])
	fmt.Printf("  %s -t 192.168.1.1 -p 1-1000 -T 500 -j 50\n", os.Args[0])
	fmt.Printf("  %s -t localhost -p common\n", os.Args[0])
}

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
			ipList = append(ipList, fmt.Sprintf("[%s]", ipv6.String()))
		}
	}
	if len(ipList) == 0 {
		return nil, fmt.Errorf("no valid IP found for %s", target)
	}
	return ipList, nil
}

func parsePorts(ports []string) []int {
	var portList []int

	for _, portInput := range ports {
		portInput = strings.TrimSpace(portInput)

		if portInput == "common" {
			commonPorts := []int{
				20, 21, 22, 23, 25, 53, 67, 68, 69, 80,
				110, 123, 135, 137, 138, 139, 143, 161, 162, 179,
				194, 389, 443, 445, 465, 514, 515, 587, 993, 995,
				1433, 1434, 1521, 1723, 2049, 2083, 2087, 3128, 3306, 3389,
				5432, 5900, 5985, 5986, 6379, 8080, 8443, 8888, 9090, 9200,
				10000, 27017,
			}
			portList = append(portList, commonPorts...)
		} else if portInput == "all" {
			for i := 1; i <= 65535; i++ {
				portList = append(portList, i)
			}
		} else if strings.Contains(portInput, "-") {
			ranges := strings.Split(portInput, "-")
			if len(ranges) != 2 {
				fmt.Printf("Invalid port range format: %s\n", portInput)
				continue
			}

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

			if start > end {
				fmt.Printf("Invalid port range: %s-%s\n", ranges[0], ranges[1])
				continue
			}

			for i := start; i <= end; i++ {
				if i >= 1 && i <= 65535 {
					portList = append(portList, i)
				}
			}
		} else {
			port, err := strconv.Atoi(portInput)
			if err != nil {
				fmt.Printf("Failed to parse port %s: %v\n", portInput, err)
				continue
			}

			if port >= 1 && port <= 65535 {
				portList = append(portList, port)
			} else {
				fmt.Printf("Port %d is out of range (1-65535)\n", port)
			}
		}
	}

	return portList
}

type ScanResult struct {
	IP     string
	Port   int
	IsOpen bool
}

func scanPort(target string, port int, timeout time.Duration) bool {
	address := fmt.Sprintf("%s:%d", target, port)
	conn, err := net.DialTimeout("tcp", address, timeout)
	if err != nil {
		return false
	}
	conn.Close()
	return true
}

func scanWorker(jobs <-chan ScanTask, results chan<- ScanResult, wg *sync.WaitGroup) {
	defer wg.Done()
	for job := range jobs {
		isOpen := scanPort(job.IP, job.Port, job.Timeout)
		results <- ScanResult{
			IP:     job.IP,
			Port:   job.Port,
			IsOpen: isOpen,
		}
	}
}

type ScanTask struct {
	IP      string
	Port    int
	Timeout time.Duration
}

func main() {
	options := parseArgs()

	var ips []string
	for _, target := range options.targets {
		ipList, err := resolve(target)
		if err != nil {
			fmt.Fprintf(os.Stderr, "Failed to resolve %s: %v\n", target, err)
			continue
		}
		ips = append(ips, ipList...)
	}

	if len(ips) == 0 {
		fmt.Fprintf(os.Stderr, "No valid targets to scan\n")
		os.Exit(1)
	}

	ports := parsePorts(options.ports)
	if len(ports) == 0 {
		fmt.Fprintf(os.Stderr, "No valid ports to scan\n")
		os.Exit(1)
	}

	totalScans := len(ips) * len(ports)

	if options.verbose {
		fmt.Printf("Scanning %d target(s) across %d port(s) with timeout %d ms using max %d threads (verbose mode)\n",
			len(ips), len(ports), options.timeout/time.Millisecond, options.maxThreads)
	} else {
		fmt.Printf("Scanning %d target(s) across %d port(s) with timeout %d ms using max %d threads\n",
			len(ips), len(ports), options.timeout/time.Millisecond, options.maxThreads)
	}

	jobs := make(chan ScanTask, options.maxThreads)
	results := make(chan ScanResult, options.maxThreads)

	var wg sync.WaitGroup
	workerCount := options.maxThreads
	if workerCount > runtime.NumCPU()*2 {
		workerCount = runtime.NumCPU() * 2
	}

	for w := 1; w <= workerCount; w++ {
		wg.Add(1)
		go scanWorker(jobs, results, &wg)
	}

	go func() {
		wg.Wait()
		close(results)
	}()

	go func() {
		for _, ip := range ips {
			for _, port := range ports {
				jobs <- ScanTask{
					IP:      ip,
					Port:    port,
					Timeout: options.timeout,
				}
			}
		}
		close(jobs)
	}()

	completedScans := 0
	for result := range results {
		completedScans++

		if result.IsOpen {
			fmt.Printf("%s:%d is open\n", result.IP, result.Port)
		} else if options.verbose {
			fmt.Printf("%s:%d is closed\n", result.IP, result.Port)
		}

		if totalScans > 10 && completedScans%10 == 0 {
			fmt.Fprintf(os.Stderr, "\rProgress: %d/%d scans completed (%d%%)",
				completedScans, totalScans, (completedScans*100)/totalScans)
		}
	}

	fmt.Fprintf(os.Stderr, "\rScan completed: %d target(s), %d port(s)\n", len(ips), len(ports))
}

// Credit: Inspired by https://medium.com/@KentGruber/building-a-high-performance-port-scanner-with-golang-9976181ec39d
// and enhanced with additional functionality
