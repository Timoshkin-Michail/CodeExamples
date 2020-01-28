package com.mikhail.timoshkin.webshop.shop.Tables.RedisHash;

import lombok.Data;
import org.springframework.data.redis.core.RedisHash;

import java.io.Serializable;
import java.util.ArrayList;
import java.util.List;

@RedisHash("Item")
@Data
public class Item implements Serializable {
    private Integer id = -1; // item id
    private String name = "";
    private Integer price = 0;
    private Integer count = 0;
    private List<String> categories = new ArrayList<>();
    private Boolean busy = false;

    @Override
    public String toString(){
        return String.format("%10d | %10s | %10d | %10d | %s", id, name, price, count, categories.toString());
    }
}
