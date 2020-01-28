package com.mikhail.timoshkin.webshop;

import lombok.Data;

import java.io.Serializable;

@Data
public class Message implements Serializable {
    private Integer id = -1;
    private String command = "";
    private String content = "";
}
