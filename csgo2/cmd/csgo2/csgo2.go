package main

import (
	"fmt"
	"net"

	"github.com/gin-gonic/gin"
)

func main() {
	router := gin.Default()
	router.StaticFile("/", "index.html")
	router.POST("/squirt", toggleSquirt)
	router.Run("0.0.0.0:8080")
}

func toggleSquirt(c *gin.Context) {
	var squirt bool
	c.BindJSON(&squirt)

	conn, err := net.Dial("tcp", "1.2.3.4:1")
	if err != nil {
		fmt.Println(err)
		c.JSON(500, gin.H{"error": "failed to connect to arduino"})
		return
	}

	if squirt {
		fmt.Println("squirtin' time")
		_, err = conn.Write([]byte("1"))
	} else {
		fmt.Println("no squirtin' time")
		_, err = conn.Write([]byte("0"))
	}

	if err != nil {
		fmt.Println(err)
		c.JSON(500, gin.H{"error": "failed to send data to arduino"})
		return
	}

	c.JSON(200, gin.H{"success": true})
}
