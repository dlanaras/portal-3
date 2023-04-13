package main

import (
	"encoding/json"
	"fmt"
	"log"
	"net"
	"os"

	"github.com/gin-gonic/gin"
)

type Config struct {
	ListenAddr        string
	ArduinoAddr       string
	IncomingFramePort int
}

func loadConfig() (*Config, error) {
	configFile, err := os.ReadFile("config.json")
	if err != nil {
		return nil, err
	}

	config := Config{
		ListenAddr:        "0.0.0.0:8080",
		ArduinoAddr:       "",
		IncomingFramePort: 8081,
	}
	err = json.Unmarshal(configFile, &config)
	if err != nil {
		return nil, err
	}

	if config.ArduinoAddr == "" {
		return nil, fmt.Errorf("ArduinoAddr not set")
	}

	return &config, nil
}

var config *Config

func main() {
	var err error
	config, err = loadConfig()
	if err != nil {
		log.Fatalln(err)
	}

	go listenForFrames()

	router := gin.Default()
	router.StaticFile("/", "index.html")
	router.POST("/squirt", toggleSquirt)
	router.Run(config.ListenAddr)
}

func listenForFrames() {
	addr, err := net.ResolveUDPAddr("udp", fmt.Sprintf("0.0.0.0:%d", config.IncomingFramePort))
	if err != nil {
		log.Fatalln(err)
	}

	conn, err := net.ListenUDP("udp", addr)
	if err != nil {
		log.Fatalln(err)
	}

	log.Println("listening for frames on", config.IncomingFramePort)

	frameBuf := make([]float64, 64)
	currentPixel := 0

	for {
		recvBuf := make([]byte, 1024)
		_, _, err := conn.ReadFromUDP(recvBuf)
		if err != nil {
			log.Println(err)
			continue
		}

		// "BEGIN" = set current pixel to 0
		// otherwise every packet is a float64 for the current pixel

		recv := string(recvBuf)

		log.Printf("got '%s'", recv)

		if recv == "BEGIN" {
			log.Println("starting new frame")
			currentPixel = 0
			continue
		}

		_, err = fmt.Sscanf(recv, "%f", &frameBuf[currentPixel])
		if err != nil {
			log.Println(err)
			continue
		}

		log.Println("got pixel", currentPixel, "with value", frameBuf[currentPixel])

		if currentPixel == 63 {
			log.Println("frame complete")
			continue
		}

		currentPixel++
	}
}

func toggleSquirt(c *gin.Context) {
	var squirt bool
	c.BindJSON(&squirt)

	conn, err := net.Dial("udp", config.ArduinoAddr+":1")
	if err != nil {
		fmt.Println(err)
		c.JSON(500, gin.H{"error": "failed to connect to arduino"})
		return
	}

	if squirt {
		log.Println("squirtin' time")
		_, err = conn.Write([]byte("1"))
	} else {
		log.Println("no squirtin' time")
		_, err = conn.Write([]byte("0"))
	}

	if err != nil {
		log.Println(err)
		c.JSON(500, gin.H{"error": "failed to send data to arduino"})
		return
	}

	c.JSON(200, gin.H{"success": true})
}
