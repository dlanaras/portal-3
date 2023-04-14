package main

import (
	"encoding/json"
	"fmt"
	"github.com/gin-gonic/gin"
	"log"
	"net"
	"os"
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
var frameBuf [64]float64

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
	router.POST("/direction", changeDirection)
	router.GET("/cam", func(c *gin.Context) {
		c.JSON(200, gin.H{"frame": frameBuf})
	})
	err = router.Run(config.ListenAddr)
	if err != nil {
		return
	}
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

	//frameBuf := make([]float64, 64)
	recvBuf := make([]byte, 1024)
	currentPixel := 0

	for {
		n, _, err := conn.ReadFromUDP(recvBuf)
		if err != nil {
			log.Println(err)
			continue
		}

		// "BEGIN" = set current pixel to 0
		// otherwise every packet is a float64 for the current pixel

		recv := string(recvBuf[:n])

		if recv == "BEGIN" {
			currentPixel = 0
			continue
		}

		if recv == "END" {
			log.Println("frame:")
			for i := 0; i < 8; i++ {
				for j := 0; j < 8; j++ {
					fmt.Printf("%f ", frameBuf[i*8+j])
				}
				fmt.Println()
			}
			continue
		}

		if currentPixel == 64 {
			log.Println("got more than 64 pixels, ignoring")
			continue
		}

		_, err = fmt.Sscanf(recv, "%f", &frameBuf[currentPixel])
		if err != nil {
			log.Println(err)
			continue
		}

		currentPixel++
	}
}

func toggleSquirt(c *gin.Context) {
	var squirt bool
	err := c.BindJSON(&squirt)
	if err != nil {
		return
	}

	conn, err := net.Dial("udp", config.ArduinoAddr+":8082")
	if err != nil {
		fmt.Println(err)
		c.JSON(500, gin.H{"error": "failed to connect to arduino"})
		return
	}

	if squirt {
		log.Println("squirtin' time")
		_, err = conn.Write([]byte("SAP"))
	} else {
		log.Println("no squirtin' time")
		_, err = conn.Write([]byte("SAD"))
	}

	if err != nil {
		log.Println(err)
		c.JSON(500, gin.H{"error": "failed to send data to arduino"})
		return
	}

	c.JSON(200, gin.H{"success": true})
}

func changeDirection(c *gin.Context) {
	var direction int
	err := c.BindJSON(&direction)
	if err != nil {
		return
	}

	conn, err := net.Dial("udp", config.ArduinoAddr+":8081")
	if err != nil {
		fmt.Println(err)
		c.JSON(500, gin.H{"error": "failed to connect to arduino"})
		return
	}

	_, err = conn.Write([]byte(fmt.Sprintf("%d", direction)))
	if err != nil {
		log.Println(err)
		c.JSON(500, gin.H{"error": "failed to send data to arduino"})
		return
	}

	c.JSON(200, gin.H{"success": true})
}
